// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "audio_effects/PitchShift.h"
#include "audio_effects/Echo.h"
#include "Clip.h"
#include "DummyReader.h"
#include "EffectInfo.h"
#include "Frame.h"
#include "Json.h"
#include "Timeline.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include <QTemporaryDir>
#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>
#include <limits>
#include <vector>

using namespace openshot;

namespace {
class ToneReader : public DummyReader {
public:
    bool silence = false;
    bool burst = false;
    bool vowel = false;
    explicit ToneReader(Fraction fps = Fraction(30, 1), int rate = 48000, int channels = 2)
        : DummyReader(fps, 16, 16, rate, channels, 4.0) {
        info.has_video = false;
        info.has_audio = true;
        info.channel_layout = channels == 1 ? LAYOUT_MONO : channels == 2 ? LAYOUT_STEREO : LAYOUT_5POINT1;
    }
    std::shared_ptr<Frame> GetFrame(int64_t number) override {
        int length = Frame::GetSamplesPerFrame(number, info.fps, info.sample_rate, info.channels);
        double raw = info.sample_rate * info.fps.Reciprocal().ToDouble() * (number - 1);
        int64_t start = std::llround(raw - std::fmod(raw, double(info.channels)));
        auto frame = std::make_shared<Frame>(number, length, info.channels);
        frame->SampleRate(info.sample_rate);
        frame->ChannelsLayout(info.channel_layout);
        std::vector<float> audio(length);
        for (int i = 0; i < length; ++i) {
            double t = double(start + i) / info.sample_rate;
            double envelope = burst ? std::exp(-std::pow((t - 1.5) / 0.04, 2)) : 1;
            double signal = 0.3 * std::sin(2 * M_PI * 440 * t);
            if (vowel) {
                signal = 0;
                for (int harmonic = 1; harmonic <= 50; ++harmonic) {
                    double hz = 100.0 * harmonic;
                    double amplitude = std::exp(-std::pow((hz - 700) / 180, 2)) +
                        0.5 * std::exp(-std::pow((hz - 1500) / 250, 2));
                    signal += 0.04 * amplitude * std::sin(2 * M_PI * hz * t);
                }
            }
            audio[i] = silence ? 0 : float(envelope * signal);
        }
        for (int ch = 0; ch < info.channels; ++ch)
            frame->AddAudio(true, ch, 0, audio.data(), length, 1.0f);
        return frame;
    }
};

std::vector<float> render(Clip& clip, int first, int last) {
    std::vector<float> audio;
    for (int n = first; n <= last; ++n) {
        auto frame = clip.GetFrame(n);
        const float* data = frame->GetAudioSamples(0);
        audio.insert(audio.end(), data, data + frame->GetAudioSamplesCount());
        for (int i = 0; i < frame->GetAudioSamplesCount(); ++i) REQUIRE(std::isfinite(data[i]));
    }
    return audio;
}

double frequency(const std::vector<float>& audio, int rate) {
    std::vector<double> crossings;
    for (size_t i = 1; i < audio.size(); ++i) {
        if (audio[i - 1] < 0 && audio[i] >= 0)
            crossings.push_back(i - 1 + double(-audio[i - 1]) / (audio[i] - audio[i - 1]));
    }
    REQUIRE(crossings.size() > 10);
    return (crossings.size() - 1) * rate / (crossings.back() - crossings.front());
}
}

TEST_CASE("PitchShift properties and factory round trip", "[audio][pitch]") {
    PitchShift effect;
    REQUIRE(effect.info.has_audio);
    REQUIRE_FALSE(effect.info.has_video);
    REQUIRE(effect.pitch.GetValue(1) == 0);
    REQUIRE(effect.mix.GetValue(1) == 1);
    REQUIRE(effect.preserve_formants);
    effect.pitch.AddPoint(1, -12, LINEAR);
    effect.pitch.AddPoint(31, 12, LINEAR);
    effect.mix.AddPoint(31, 0.5, LINEAR);
    effect.quality = PitchShift::HIGH;
    effect.preserve_formants = false;
    std::unique_ptr<EffectBase> restored(EffectInfo().CreateEffect("PitchShift"));
    REQUIRE(restored);
    restored->SetJson(effect.Json());
    REQUIRE(restored->JsonValue() == effect.JsonValue());
    auto properties = stringToJson(restored->PropertiesJSON(16));
    REQUIRE(properties["pitch"]["points"].asInt() == 2);
    REQUIRE(properties["mix"]["points"].asInt() == 2);
    REQUIRE(properties["quality"]["choices"].size() == 3);
    REQUIRE_FALSE(properties["quality"]["keyframe"].asBool());
    REQUIRE_FALSE(properties["preserve_formants"]["keyframe"].asBool());
}

TEST_CASE("Fractional audio automation preserves interpolation", "[audio][pitch][keyframe]") {
    Keyframe curve(0);
    curve.AddPoint(11, 12, LINEAR);
    REQUIRE(curve.GetValueAtFrame(1.5) == Approx(0.6));
    REQUIRE(curve.GetValueAtFrame(5.25) == Approx(5.1));
    curve.UpdatePoint(1, Point(11, 12, CONSTANT));
    REQUIRE(curve.GetValueAtFrame(10.999) == 0);
    REQUIRE(curve.GetValueAtFrame(11) == 12);
    curve.UpdatePoint(1, Point(11, 12, BEZIER));
    for (int n = 1; n <= 12; ++n)
        REQUIRE(curve.GetValueAtFrame(n) == curve.GetValue(n));
}

TEST_CASE("Neutral pitch and dry mix preserve exact samples", "[audio][pitch]") {
    ToneReader reader;
    Clip clip(&reader);
    PitchShift effect;
    clip.AddEffect(&effect);
    clip.Open();
    for (bool dry : {false, true}) {
        effect.pitch = Keyframe(dry ? 12 : 0);
        effect.mix = Keyframe(dry ? 0 : 1);
        auto input = reader.GetFrame(20);
        auto output = clip.GetFrame(20);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < input->GetAudioSamplesCount(); ++i)
                REQUIRE(input->GetAudioSamples(ch)[i] == output->GetAudioSamples(ch)[i]);
    }
}

TEST_CASE("Pitch shifts by musical intervals without changing duration", "[audio][pitch]") {
    const double semitones = GENERATE(-24.0, -12.0, -3.5, 7.0, 12.0, 24.0);
    ToneReader reader;
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch = Keyframe(semitones);
    effect.preserve_formants = false;
    clip.AddEffect(&effect);
    clip.Open();
    auto audio = render(clip, 16, 45);
    REQUIRE(audio.size() == 48000);
    REQUIRE(frequency(audio, 48000) == Approx(440 * std::pow(2, semitones / 12)).margin(2.0));
}

TEST_CASE("Quality modes support mono stereo and fractional frame rates", "[audio][pitch]") {
    const int mode = GENERATE(int(PitchShift::FAST), int(PitchShift::BALANCED), int(PitchShift::HIGH));
    const int channels = GENERATE(1, 2, 6);
    ToneReader reader(Fraction(30000, 1001), 44100, channels);
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch = Keyframe(5);
    effect.quality = mode;
    clip.AddEffect(&effect);
    clip.Open();
    int total = 0;
    double error = 0, energy = 0;
    for (int n = 20; n < 35; ++n) {
        auto frame = clip.GetFrame(n);
        REQUIRE(frame->GetAudioSamplesCount() == Frame::GetSamplesPerFrame(n, reader.info.fps, 44100, channels));
        total += frame->GetAudioSamplesCount();
        for (int i = 0; i < frame->GetAudioSamplesCount(); ++i) {
            REQUIRE(std::isfinite(frame->GetAudioSamples(0)[i]));
            for (int ch = 1; ch < channels; ++ch) {
                double difference = frame->GetAudioSamples(ch)[i] - frame->GetAudioSamples(0)[i];
                error += difference * difference;
                energy += double(frame->GetAudioSamples(0)[i]) * frame->GetAudioSamples(0)[i];
            }
        }
    }
    REQUIRE(total > 22000);
    if (channels > 1) REQUIRE(error / energy < 1e-6);
}

TEST_CASE("Pitch seeking repeated requests and edits do not reuse stale audio", "[audio][pitch]") {
    ToneReader reader;
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch = Keyframe(12);
    clip.AddEffect(&effect);
    clip.Open();
    auto first = clip.GetFrame(40);
    auto repeat = clip.GetFrame(40);
    for (int i = 0; i < first->GetAudioSamplesCount(); ++i)
        REQUIRE(first->GetAudioSamples(0)[i] == repeat->GetAudioSamples(0)[i]);
    render(clip, 70, 72);
    REQUIRE(frequency(render(clip, 20, 35), 48000) == Approx(880).margin(2));
    effect.pitch = Keyframe(-12);
    REQUIRE(frequency(render(clip, 20, 35), 48000) == Approx(220).margin(2));
    effect.pitch = Keyframe(0);
    auto neutral = clip.GetFrame(21);
    auto source = reader.GetFrame(21);
    REQUIRE(neutral->GetAudioSamples(0)[100] == source->GetAudioSamples(0)[100]);
}

TEST_CASE("Pitch compensation keeps an audio event at its original time", "[audio][pitch]") {
    ToneReader reader;
    reader.burst = true;
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch = Keyframe(7);
    clip.AddEffect(&effect);
    clip.Open();
    auto audio = render(clip, 1, 90);
    double weighted_time = 0, energy = 0;
    for (size_t i = 0; i < audio.size(); ++i) {
        double power = double(audio[i]) * audio[i];
        weighted_time += (double(i) / 48000) * power;
        energy += power;
    }
    REQUIRE(energy > 1);
    REQUIRE(weighted_time / energy == Approx(1.5).margin(0.015));
}

TEST_CASE("Pitch silence and clip boundaries remain finite", "[audio][pitch]") {
    ToneReader reader;
    reader.silence = true;
    Clip clip(&reader);
    clip.Start(1);
    clip.End(2);
    PitchShift effect;
    effect.pitch = Keyframe(-24);
    clip.AddEffect(&effect);
    clip.Open();
    for (int n : {31, 60, 32, 59}) {
        auto frame = clip.GetFrame(n);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < frame->GetAudioSamplesCount(); ++i)
                REQUIRE(frame->GetAudioSamples(ch)[i] == 0);
    }
}

TEST_CASE("Pitch mix blends aligned wet and dry audio", "[audio][pitch]") {
    ToneReader wet_reader, mixed_reader;
    Clip wet_clip(&wet_reader), mixed_clip(&mixed_reader);
    PitchShift wet, mixed;
    wet.pitch = Keyframe(7);
    mixed.pitch = Keyframe(7);
    mixed.mix = Keyframe(0.3);
    wet_clip.AddEffect(&wet);
    mixed_clip.AddEffect(&mixed);
    wet_clip.Open();
    mixed_clip.Open();
    auto a = wet_clip.GetFrame(30);
    auto b = mixed_clip.GetFrame(30);
    auto dry = mixed_reader.GetFrame(30);
    for (int i = 0; i < a->GetAudioSamplesCount(); ++i)
        REQUIRE(b->GetAudioSamples(0)[i] == Approx(0.7 * dry->GetAudioSamples(0)[i] + 0.3 * a->GetAudioSamples(0)[i]).margin(1e-6));
}

TEST_CASE("Pitch automation reaches its musical targets", "[audio][pitch]") {
    ToneReader reader;
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch.AddPoint(1, -12, LINEAR);
    effect.pitch.AddPoint(31, -12, LINEAR);
    effect.pitch.AddPoint(61, 12, LINEAR);
    effect.pitch.AddPoint(100, 12, LINEAR);
    clip.AddEffect(&effect);
    clip.Open();
    REQUIRE(frequency(render(clip, 16, 25), 48000) == Approx(220).margin(2));
    auto glide = render(clip, 26, 70);
    // Check energy and continuity across video-frame boundaries during the bend.
    double energy = 0;
    float largest_jump = 0;
    for (size_t i = 1; i < glide.size(); ++i) {
        energy += double(glide[i]) * glide[i];
        largest_jump = std::max(largest_jump, std::abs(glide[i] - glide[i - 1]));
    }
    REQUIRE(energy / glide.size() > 0.005);
    REQUIRE(largest_jump < 0.15);
    REQUIRE(frequency(render(clip, 71, 85), 48000) == Approx(880).margin(2));
}

TEST_CASE("Pitch context respects earlier audio effects without advancing their state", "[audio][pitch]") {
    ToneReader reader;
    reader.burst = true;
    Clip clip(&reader);
    Echo echo;
    echo.echo_time = Keyframe(0.2);
    echo.feedback = Keyframe(0);
    echo.mix = Keyframe(1);
    echo.Order(2);
    PitchShift effect;
    effect.pitch = Keyframe(7);
    effect.Order(1);
    clip.AddEffect(&echo);
    clip.AddEffect(&effect);
    clip.Open();
    auto audio = render(clip, 1, 90);
    double energy = 0, weighted_time = 0;
    for (size_t i = 0; i < audio.size(); ++i) {
        double power = double(audio[i]) * audio[i];
        energy += power;
        weighted_time += double(i) / 48000 * power;
    }
    REQUIRE(energy > 1);
    REQUIRE(weighted_time / energy == Approx(1.7).margin(0.015));
    // The original echo still has exactly the state of 90 normally processed frames.
    // Removing pitch should not reveal state advanced by the pitch look-ahead.
    clip.RemoveEffect(&effect);
    auto frame = clip.GetFrame(91);
    for (int i = 0; i < frame->GetAudioSamplesCount(); ++i)
        REQUIRE(std::abs(frame->GetAudioSamples(0)[i]) < 1e-6);
}

TEST_CASE("Pitch repeated seeks reconstruct recent audio deterministically", "[audio][pitch]") {
    ToneReader reader;
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch = Keyframe(7);
    clip.AddEffect(&effect);
    clip.Open();
    auto a = clip.GetFrame(60);
    clip.GetFrame(10);
    auto b = clip.GetFrame(60);
    for (int i = 0; i < a->GetAudioSamplesCount(); ++i)
        REQUIRE(a->GetAudioSamples(0)[i] == b->GetAudioSamples(0)[i]);
}

TEST_CASE("Pitch render throughput", "[.benchmark][pitch]") {
    for (int quality : {PitchShift::FAST, PitchShift::BALANCED, PitchShift::HIGH}) {
        ToneReader reader;
        Clip clip(&reader);
        PitchShift effect;
        effect.pitch = Keyframe(7);
        effect.quality = quality;
        clip.AddEffect(&effect);
        clip.Open();
        auto start = std::chrono::steady_clock::now();
        auto audio = render(clip, 1, 90);
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Pitch quality=" << quality << ": 3 seconds of stereo audio in "
            << elapsed << " seconds (" << 3 / elapsed << "x realtime)\n";
        REQUIRE(audio.size() == 144000);
    }
}

TEST_CASE("Formant preservation retains the spectral envelope of a voice", "[audio][pitch]") {
    auto centroid = [](const std::vector<float>& audio) {
        double weighted = 0, energy = 0;
        // Measure energy at the shifted harmonics of the synthetic 100 Hz voice.
        for (int hz = 200; hz <= 6000; hz += 200) {
            double real = 0, imaginary = 0;
            for (size_t i = 0; i < audio.size(); ++i) {
                double angle = 2 * M_PI * hz * i / 48000;
                real += audio[i] * std::cos(angle);
                imaginary += audio[i] * std::sin(angle);
            }
            double power = real * real + imaginary * imaginary;
            energy += power;
            weighted += hz * power;
        }
        REQUIRE(energy > 1);
        return weighted / energy;
    };
    ToneReader reader;
    reader.vowel = true;
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch = Keyframe(12);
    clip.AddEffect(&effect);
    clip.Open();
    effect.preserve_formants = false;
    double shifted = centroid(render(clip, 31, 45));
    effect.preserve_formants = true;
    double preserved = centroid(render(clip, 31, 45));
    INFO("Shifted centroid " << shifted << ", preserved centroid " << preserved);
    REQUIRE(preserved < shifted * 0.8);
    REQUIRE(preserved > shifted * 0.35);
}

TEST_CASE("Pitch works through timeline frame and sample rate conversion", "[audio][pitch][timeline]") {
    const double semitones = GENERATE(0.0, 12.0);
    INFO("Timeline shift " << semitones);
    ToneReader reader(Fraction(24, 1), 48000, 2);
    Clip clip(&reader);
    PitchShift effect;
    effect.pitch = Keyframe(semitones);
    clip.AddEffect(&effect);
    Timeline timeline(16, 16, Fraction(30000, 1001), 44100, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    std::vector<float> audio;
    for (int n = 20; n < 50; ++n) {
        auto frame = timeline.GetFrame(n);
        REQUIRE(frame->GetAudioSamplesCount() == Frame::GetSamplesPerFrame(n, timeline.info.fps, 44100, 2));
        audio.insert(audio.end(), frame->GetAudioSamples(0), frame->GetAudioSamples(0) + frame->GetAudioSamplesCount());
    }
    REQUIRE(frequency(audio, 44100) == Approx(440 * std::pow(2, semitones / 12)).margin(3));
    timeline.Close();
}

TEST_CASE("Pitch composes with clip speed changes", "[audio][pitch][time]") {
    ToneReader reader;
    Clip clip(&reader);
    clip.time.AddPoint(1, 1, LINEAR);
    clip.time.AddPoint(61, 121, LINEAR);
    PitchShift effect;
    effect.pitch = Keyframe(-12);
    effect.preserve_formants = false;
    clip.AddEffect(&effect);
    clip.Open();
    auto audio = render(clip, 16, 45);
    REQUIRE(audio.size() == 48000);
    REQUIRE(frequency(audio, 48000) == Approx(440).margin(3));
}

TEST_CASE("Pitch context leaves preceding effect playback state untouched", "[audio][pitch]") {
    ToneReader source, reference;
    Clip clip(&source), reference_clip(&reference);
    Echo echo, reference_echo;
    echo.echo_time = reference_echo.echo_time = Keyframe(0.15);
    echo.mix = reference_echo.mix = Keyframe(1);
    echo.feedback = reference_echo.feedback = Keyframe(0.3);
    echo.Order(2);
    PitchShift pitch;
    pitch.pitch = Keyframe(7);
    pitch.Order(1);
    clip.AddEffect(&echo);
    clip.AddEffect(&pitch);
    reference_clip.AddEffect(&reference_echo);
    clip.Open();
    reference_clip.Open();
    render(clip, 1, 40);
    render(reference_clip, 1, 40);
    clip.RemoveEffect(&pitch);
    auto actual = clip.GetFrame(41);
    auto expected = reference_clip.GetFrame(41);
    for (int i = 0; i < actual->GetAudioSamplesCount(); ++i)
        REQUIRE(actual->GetAudioSamples(0)[i] == expected->GetAudioSamples(0)[i]);
}

TEST_CASE("Pitch exports real audio at the requested rate and duration", "[audio][pitch][export]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = directory.filePath("pitch.wav").toStdString();
    Clip clip(std::string(TEST_MEDIA_PATH) + "piano.wav");
    PitchShift effect;
    effect.pitch = Keyframe(7);
    clip.AddEffect(&effect);
    Timeline timeline(16, 16, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    FFmpegWriter writer(path);
    writer.SetAudioOptions(true, "pcm_f32le", 48000, 2, LAYOUT_STEREO, 3072000);
    writer.Open();
    writer.WriteFrame(&timeline, 1, 90);
    writer.Close();
    timeline.Close();
    FFmpegReader result(path);
    result.Open();
    REQUIRE(result.info.sample_rate == 48000);
    REQUIRE(result.info.channels == 2);
    REQUIRE(result.info.duration == Approx(3).margin(1.0 / 48000));
    auto frame = result.GetFrame(30);
    double energy = 0;
    for (int i = 0; i < frame->GetAudioSamplesCount(); ++i) {
        double value = frame->GetAudioSamples(0)[i];
        REQUIRE(std::isfinite(value));
        energy += value * value;
    }
    REQUIRE(energy > 0.001);
    result.Close();
}

TEST_CASE("Mix keyframes follow fractional audio time", "[audio][pitch][keyframe]") {
    ToneReader wet_reader, mixed_reader;
    Clip wet_clip(&wet_reader), mixed_clip(&mixed_reader);
    PitchShift wet, mixed;
    wet.pitch = mixed.pitch = Keyframe(7);
    mixed.mix.AddPoint(1, 0, LINEAR);
    mixed.mix.AddPoint(31, 1, LINEAR);
    wet_clip.AddEffect(&wet);
    mixed_clip.AddEffect(&mixed);
    wet_clip.Open();
    mixed_clip.Open();
    auto a = wet_clip.GetFrame(20);
    auto b = mixed_clip.GetFrame(20);
    auto dry = mixed_reader.GetFrame(20);
    const int length = a->GetAudioSamplesCount();
    for (int i = 0; i < length; ++i) {
        double amount = (19 + double(i) / length) / 30;
        double expected = dry->GetAudioSamples(0)[i] * (1 - amount) + a->GetAudioSamples(0)[i] * amount;
        REQUIRE(b->GetAudioSamples(0)[i] == Approx(expected).margin(1e-6));
    }
}

TEST_CASE("Pitch remains continuous on moved and trimmed fractional FPS clips", "[audio][pitch][timeline]") {
    ToneReader reader(Fraction(24, 1), 48000, 2), baseline_reader(Fraction(24, 1), 48000, 2);
    Clip clip(&reader), baseline_clip(&baseline_reader);
    clip.Start(11 * 1001.0 / 30000);
    clip.End(3.5);
    clip.Position(63 * 1001.0 / 30000);
    baseline_clip.Start(clip.Start());
    baseline_clip.End(clip.End());
    baseline_clip.Position(clip.Position());
    Timeline baseline(16, 16, Fraction(30000, 1001), 44100, 2, LAYOUT_STEREO);
    baseline.AddClip(&baseline_clip);
    baseline.Open();
    PitchShift effect;
    effect.pitch = Keyframe(12);
    clip.AddEffect(&effect);
    Timeline timeline(16, 16, Fraction(30000, 1001), 44100, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    std::vector<float> audio;
    float largest_boundary_jump = 0;
    for (int n = 70; n < 100; ++n) {
        auto frame = timeline.GetFrame(n);
        const float* samples = frame->GetAudioSamples(0);
        REQUIRE(frame->GetAudioSamplesCount() == baseline.GetFrame(n)->GetAudioSamplesCount());
        if (!audio.empty()) largest_boundary_jump = std::max(largest_boundary_jump, std::abs(audio.back() - samples[0]));
        audio.insert(audio.end(), samples, samples + frame->GetAudioSamplesCount());
    }
    REQUIRE(frequency(audio, 44100) == Approx(880).margin(3));
    REQUIRE(largest_boundary_jump < 0.15);
    timeline.Close();
    baseline.Close();
}

TEST_CASE("Empty pitch curves and clamped dry mix preserve the original samples", "[audio][pitch]") {
    ToneReader reader;
    Clip clip(&reader);
    PitchShift effect;
    clip.AddEffect(&effect);
    clip.Open();
    for (bool empty_pitch : {true, false}) {
        effect.pitch = empty_pitch ? Keyframe() : Keyframe(12);
        effect.mix = Keyframe(empty_pitch ? 1 : -1);
        auto source = reader.GetFrame(20);
        auto actual = clip.GetFrame(20);
        for (int ch = 0; ch < source->GetAudioChannelsCount(); ++ch)
            for (int i = 0; i < source->GetAudioSamplesCount(); ++i)
                REQUIRE(actual->GetAudioSamples(ch)[i] == source->GetAudioSamples(ch)[i]);
    }
}

TEST_CASE("Stacked pitch effects follow effect order and preserve musical intervals", "[audio][pitch]") {
    ToneReader reader;
    Clip clip(&reader);
    PitchShift first, second;
    first.pitch = Keyframe(7);
    first.preserve_formants = false;
    first.Order(2);
    second.pitch = Keyframe(5);
    second.preserve_formants = false;
    second.Order(1);
    clip.AddEffect(&first);
    clip.AddEffect(&second);
    clip.Open();
    REQUIRE(frequency(render(clip, 16, 45), 48000) == Approx(880).margin(3));
    second.pitch = Keyframe(-7);
    REQUIRE(frequency(render(clip, 16, 45), 48000) == Approx(440).margin(3));
}
