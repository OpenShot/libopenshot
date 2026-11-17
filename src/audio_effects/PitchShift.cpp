/** @file @brief Pitch shifting with compensated look-ahead and musical automation. */
// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PitchShift.h"
#include "Clip.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "Json.h"
#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <vector>

// Keep third-party implementation details out of installed public headers.
#include "stretch/signalsmith-stretch.h"

using namespace openshot;

namespace {
constexpr int block_size = 128;

double bounded(double value, double low, double high, double fallback) {
    return std::isfinite(value) ? std::max(low, std::min(high, value)) : fallback;
}

// Match Frame::GetSamplesPerFrame, including channel-aligned fractional FPS.
int64_t sample_start(int64_t number, Fraction fps, int rate, int channels, int64_t offset) {
    const double value = rate * fps.Reciprocal().ToDouble() * (number + offset - 1);
    return static_cast<int64_t>(std::llround(value - std::fmod(value, double(channels))));
}

int64_t sample_frame(int64_t sample, Fraction fps, int rate, int channels, int64_t offset) {
    auto number = static_cast<int64_t>(std::floor(sample * fps.ToDouble() / rate)) + 1 - offset;
    while (sample_start(number, fps, rate, channels, offset) > sample) --number;
    while (sample_start(number + 1, fps, rate, channels, offset) <= sample) ++number;
    return number;
}

double frame_at(int64_t sample, Fraction fps, int rate, int channels, int64_t offset) {
    const auto number = sample_frame(sample, fps, rate, channels, offset);
    const auto start = sample_start(number, fps, rate, channels, offset);
    const auto length = sample_start(number + 1, fps, rate, channels, offset) - start;
    return number + double(sample - start) / std::max<int64_t>(1, length);
}
}

struct PitchShift::State {
    std::recursive_mutex mutex;
    using Processor = signalsmith::stretch::SignalsmithStretch<float>;
    std::unique_ptr<Processor> processor;
    // Effects are destroyed before their borrowed-reader context.
    std::unique_ptr<Clip> context;
    std::vector<std::unique_ptr<EffectBase>> effects;
    std::map<int64_t, std::shared_ptr<Frame>> source_frames;
    std::string signature;
    int64_t input_position = 0, output_position = 0;
    int64_t last_frame = -1;
    bool initialized = false;
    juce::AudioBuffer<float> input, output, wet;
    std::vector<const float*> input_pointers;
    std::vector<float*> output_pointers;
    std::vector<float> mix_amounts;
};

PitchShift::PitchShift() : pitch(0.0), mix(1.0), state(new State()) {
    InitEffectInfo();
    info.class_name = "PitchShift";
    info.name = "Pitch Shift";
    info.description = "Raise or lower audio pitch without changing its speed or duration.";
    info.has_audio = true;
}

PitchShift::~PitchShift() = default;

std::shared_ptr<Frame> PitchShift::GetFrame(int64_t number) {
    return GetFrame(std::make_shared<Frame>(), number);
}

std::shared_ptr<Frame> PitchShift::GetFrame(std::shared_ptr<Frame> frame, int64_t number) {
    const std::lock_guard<std::recursive_mutex> lock(state->mutex);
    auto& s = *state;
    const int channels = frame->GetAudioChannelsCount();
    const int samples = frame->GetAudioSamplesCount();
    const int rate = frame->SampleRate();
    if (channels <= 0 || samples <= 0 || rate <= 0) return frame;

    // Only bypass an entirely neutral curve. Keep state through animated zeroes.
    if ((pitch.GetCount() <= 1 && bounded(pitch.GetValue(1), -24, 24, 0) == 0) ||
        (mix.GetCount() <= 1 && bounded(mix.GetValue(1), 0, 1, 1) == 0)) {
        s.initialized = false;
        s.last_frame = -1;
        return frame;
    }

    auto* parent = dynamic_cast<Clip*>(ParentClip());
    Json::Value context_settings;
    Fraction fps(rate, samples);
    if (parent && parent->Reader()) {
        context_settings = parent->AudioContextJson(this);
        fps = parent->Reader()->info.fps;
    }
    if (fps.num <= 0 || fps.den <= 0 || rate * fps.Reciprocal().ToDouble() < channels)
        return frame;

    // FrameMapper distributes fractional-frame sample counts using timeline
    // position. Keep that same axis for moved/trimmed clips, then map automation
    // back to clip frames. Otherwise two-sample differences can reset DSP every frame.
    int64_t offset = 0;
    if (parent) {
        if (parent->time.GetLength() > 1)
            offset = parent->adjust_timeline_framenumber(1) - 1;
        else if (auto* mapper = dynamic_cast<FrameMapper*>(parent->Reader()))
            offset = mapper->AdjustFrameNumber(1) - 1;
    }

    auto settings = JsonValue();
    settings["context"] = context_settings;
    settings["rate"] = rate;
    settings["channels"] = channels;
    settings["fps_num"] = fps.num;
    settings["fps_den"] = fps.den;
    const std::string signature = settings.toStyledString();
    const int64_t start = sample_start(number, fps, rate, channels, offset);
    const bool reset = !s.initialized || signature != s.signature ||
        (number != s.last_frame && start != s.output_position) || !parent;

    if (reset) {
        s.source_frames.clear();
        s.effects.clear();
        s.context.reset();
        s.signature = signature;
        s.processor.reset(new State::Processor(0));
        const int mode = std::max(int(FAST), std::min(int(HIGH), quality));
        // The window determines frequency resolution; overlap determines how
        // often spectral analysis runs. Never rebuild these for pitch keyframes.
        if (mode == FAST) s.processor->presetCheaper(channels, rate, false);
        else if (mode == HIGH) s.processor->configure(channels, int(rate * 0.12), int(rate * 0.015));
        else s.processor->presetDefault(channels, rate, false);
        s.processor->reset();
        s.processor->setFormantFactor(1.0f, preserve_formants);
        s.input.setSize(channels, block_size);
        s.output.setSize(channels, block_size);
        s.input_pointers.resize(channels);
        s.output_pointers.resize(channels);
        for (int channel = 0; channel < channels; ++channel) {
            s.input_pointers[channel] = s.input.getReadPointer(channel);
            s.output_pointers[channel] = s.output.getWritePointer(channel);
        }
        if (parent) s.context = parent->CreateAudioContext(context_settings, s.effects);
        // Warm spectral and upstream effect state with bounded pre-roll on a
        // seek. Sequential requests retain state instead of repeating this work.
        const int preroll = s.processor->blockSamples() * 4;
        const int64_t first = parent ? std::max<int64_t>(1,
            std::llround(parent->Start() * fps.ToDouble()) + 1) : number;
        s.input_position = std::max(sample_start(first, fps, rate, channels, offset), start - preroll);
        s.output_position = s.input_position - s.processor->inputLatency() - s.processor->outputLatency();
        s.initialized = true;
        s.last_frame = -1;
    }

    if (s.last_frame != number) {
        s.wet.setSize(channels, samples, false, false, true);
        auto read_input = [&](int64_t position, int count) {
            s.input.clear();
            int copied = 0;
            while (copied < count) {
                const auto n = sample_frame(position + copied, fps, rate, channels, offset);
                const auto source_offset = int(position + copied - sample_start(n, fps, rate, channels, offset));
                const int available = int(sample_start(n + 1, fps, rate, channels, offset) -
                    sample_start(n, fps, rate, channels, offset)) - source_offset;
                const int take = std::min(count - copied, available);
                std::shared_ptr<Frame> source;
                if (s.context) {
                    auto found = s.source_frames.find(n);
                    if (found == s.source_frames.end()) {
                        source = s.context->GetAudioContextFrame(n);
                        s.source_frames[n] = source;
                    } else source = found->second;
                } else if (n == number) source = frame;
                if (source && source_offset < source->GetAudioSamplesCount()) {
                    const int length = std::min(take, source->GetAudioSamplesCount() - source_offset);
                    for (int ch = 0; ch < std::min(channels, source->GetAudioChannelsCount()); ++ch)
                        s.input.copyFrom(ch, copied, source->GetAudioSamples(ch) + source_offset, length);
                }
                copied += take;
            }
            // Retain the two most recent source frames; reader caching handles look-ahead.
            while (s.source_frames.size() > 2) s.source_frames.erase(s.source_frames.begin());
        };
        const int64_t end = start + samples;
        while (s.output_position < end) {
            int count = int(std::min<int64_t>(block_size, end - s.output_position));
            if (s.output_position < start) count = int(std::min<int64_t>(count, start - s.output_position));
            read_input(s.input_position, count);
            // Automation is centred on processing time, ahead of the output by
            // outputLatency(), rather than applied late at the video boundary.
            const double automation_frame = frame_at(s.input_position - s.processor->inputLatency() + count / 2,
                fps, rate, channels, offset);
            s.processor->setTransposeSemitones(float(bounded(pitch.GetValueAtFrame(automation_frame), -24, 24, 0)));
            s.processor->process(s.input_pointers.data(), count, s.output_pointers.data(), count);
            if (s.output_position >= start) {
                for (int ch = 0; ch < channels; ++ch)
                    s.wet.copyFrom(ch, int(s.output_position - start), s.output, ch, 0, count);
            }
            s.input_position += count;
            s.output_position += count;
        }
        s.last_frame = number;
    }

    // The wet stream is latency-compensated; dry samples need no delay.
    // Constant mix is the common path: evaluate it once, outside the sample loop.
    const bool animated_mix = mix.GetCount() > 1;
    const float constant_mix = float(bounded(mix.GetValue(number), 0, 1, 1));
    if (animated_mix) {
        s.mix_amounts.resize(samples);
        for (int i = 0; i < samples; ++i)
            s.mix_amounts[i] = float(bounded(mix.GetValueAtFrame(frame_at(start + i, fps, rate, channels, offset)), 0, 1, 1));
    }
    for (int ch = 0; ch < channels; ++ch) {
        float* dry = frame->audio->getWritePointer(ch);
        const float* wet = s.wet.getReadPointer(ch);
        for (int i = 0; i < samples; ++i) {
            const float amount = animated_mix ? s.mix_amounts[i] : constant_mix;
            dry[i] += amount * (wet[i] - dry[i]);
        }
    }
    return frame;
}

std::string PitchShift::Json() const { return JsonValue().toStyledString(); }
Json::Value PitchShift::JsonValue() const {
    auto root = EffectBase::JsonValue();
    root["type"] = info.class_name;
    root["pitch"] = pitch.JsonValue();
    root["mix"] = mix.JsonValue();
    root["preserve_formants"] = preserve_formants;
    root["quality"] = quality;
    return root;
}
void PitchShift::SetJson(const std::string value) {
    SetJsonValue(openshot::stringToJson(value));
}
void PitchShift::SetJsonValue(const Json::Value root) {
    const std::lock_guard<std::recursive_mutex> lock(state->mutex);
    EffectBase::SetJsonValue(root);
    if (!root["pitch"].isNull()) pitch.SetJsonValue(root["pitch"]);
    if (!root["mix"].isNull()) mix.SetJsonValue(root["mix"]);
    if (!root["preserve_formants"].isNull()) preserve_formants = root["preserve_formants"].asBool();
    if (!root["quality"].isNull()) quality = std::max(int(FAST), std::min(int(HIGH), root["quality"].asInt()));
}
std::string PitchShift::PropertiesJSON(int64_t number) const {
    auto root = BasePropertiesJSON(number);
    root["pitch"] = add_property_json("Pitch (semitones)", pitch.GetValue(number), "float", "", &pitch, -24, 24, false, number);
    root["mix"] = add_property_json("Mix", mix.GetValue(number), "float", "", &mix, 0, 1, false, number);
    root["preserve_formants"] = add_property_json("Preserve Formants", preserve_formants, "int", "", nullptr, 0, 1, false, number);
    root["preserve_formants"]["choices"].append(add_property_choice_json("Off", 0, preserve_formants));
    root["preserve_formants"]["choices"].append(add_property_choice_json("On", 1, preserve_formants));
    root["quality"] = add_property_json("Quality", quality, "int", "", nullptr, 0, 2, false, number);
    root["quality"]["choices"].append(add_property_choice_json("Fast", FAST, quality));
    root["quality"]["choices"].append(add_property_choice_json("Balanced", BALANCED, quality));
    root["quality"]["choices"].append(add_property_choice_json("High", HIGH, quality));
    return root.toStyledString();
}
