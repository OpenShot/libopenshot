/**
 * @file
 * @brief Source file for AudioWaveformer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioWaveformer.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "Clip.h"
#include "Exceptions.h"
#include "FrameMapper.h"
#include "FFmpegReader.h"
#include "Timeline.h"


using namespace std;
using namespace openshot;


namespace {
// Waveform extraction reads every frame exactly once, front to back, so a large
// reader cache only costs memory (and, because CacheMemory sizes are rescanned on
// every insert, time). Keep readers owned by the waveformer nearly cache-free.
constexpr int64_t WAVEFORM_READER_CACHE_BYTES = 2 * 1024 * 1024;

void LimitWaveformReaderCache(ReaderBase* reader)
{
	if (!reader) {
		return;
	}
	if (CacheBase* cache = reader->GetCache()) {
		cache->SetMaxBytes(WAVEFORM_READER_CACHE_BYTES);
	}
}

// Linearly interpolate a keyframe at a fractional (1-based) frame position
double KeyframeValueAt(const Keyframe& keyframe, double frame)
{
	const double floor_frame = std::floor(frame);
	const int64_t index = static_cast<int64_t>(floor_frame);
	const double value = keyframe.GetValue(index);
	const double fraction = frame - floor_frame;
	if (fraction <= 0.0) {
		return value;
	}
	return value + (keyframe.GetValue(index + 1) - value) * fraction;
}
}

// Default constructor
AudioWaveformer::AudioWaveformer(ReaderBase* new_reader) :
	reader(new_reader),
	detached_reader(nullptr),
	resolved_reader(nullptr),
	source_initialized(false)
{

}

// Destructor
AudioWaveformer::~AudioWaveformer()
{

}

// Extract audio samples from any ReaderBase class
AudioWaveformData AudioWaveformer::ExtractSamples(int channel, int num_per_second, bool normalize) {
	// Legacy entry point: resolve a source reader (unwrap Clip/FrameMapper), then extract audio-only.
	AudioWaveformData data;
	if (!reader) {
		return data;
	}

	ReaderBase* source = ResolveWaveformReader();

	Fraction source_fps = ResolveSourceFPS(source);

	AudioWaveformData base = ExtractSamplesFromReader(source, channel, num_per_second, false);

	// If this is a Clip, apply its keyframes using project fps (timeline if available, else reader fps)
	if (auto clip = dynamic_cast<Clip*>(reader)) {
		Timeline* timeline = dynamic_cast<Timeline*>(clip->ParentTimeline());
		Fraction project_fps = timeline ? timeline->info.fps : clip->Reader()->info.fps;
		return ApplyKeyframes(base, &clip->time, &clip->volume, project_fps, source_fps, source->info.channels, num_per_second, channel, normalize);
	}

	// No keyframes to apply
	if (normalize) {
		float max_sample = 0.0f;
		for (auto v : base.max_samples) {
			max_sample = std::max(max_sample, std::abs(v));
		}
		if (max_sample > 0.0f) {
			base.scale(static_cast<int>(base.max_samples.size()), 1.0f / max_sample);
		}
	}
	return base;
}

// Extract the mixed envelope plus one envelope per channel, in a single decode pass
AudioWaveformData AudioWaveformer::ExtractChannelSamples(int num_per_second, bool normalize) {
	AudioWaveformData data;
	if (!reader) {
		return data;
	}
	ReaderBase* source = ResolveWaveformReader();
	return ExtractSamplesFromReader(source, -1, num_per_second, normalize, true);
}

AudioWaveformData AudioWaveformer::ExtractSamples(const std::string& path, int channel, int num_per_second, bool normalize) {
	FFmpegReader temp_reader(path);
	temp_reader.Open();
	LimitWaveformReaderCache(&temp_reader);
	// Disable video for speed
	bool has_video = temp_reader.info.has_video;
	temp_reader.info.has_video = false;
	AudioWaveformData data = ExtractSamplesFromReader(&temp_reader, channel, num_per_second, normalize);
	temp_reader.info.has_video = has_video;
	temp_reader.Close();
	return data;
}

AudioWaveformData AudioWaveformer::ExtractSamples(const std::string& path,
												  const Keyframe* time_keyframe,
												  const Keyframe* volume_keyframe,
												  const Fraction& project_fps,
												  int channel,
												  int num_per_second,
												  bool normalize) {
	FFmpegReader temp_reader(path);
	temp_reader.Open();
	LimitWaveformReaderCache(&temp_reader);
	bool has_video = temp_reader.info.has_video;
	temp_reader.info.has_video = false;
	Fraction source_fps = temp_reader.info.fps;
	AudioWaveformData base = ExtractSamplesFromReader(&temp_reader, channel, num_per_second, false);
	temp_reader.info.has_video = has_video;
	temp_reader.Close();
	return ApplyKeyframes(base, time_keyframe, volume_keyframe, project_fps, source_fps, temp_reader.info.channels, num_per_second, channel, normalize);
}

AudioWaveformData AudioWaveformer::ApplyKeyframes(const AudioWaveformData& base,
												  const Keyframe* time_keyframe,
												  const Keyframe* volume_keyframe,
												  const Fraction& project_fps,
												  const Fraction& source_fps,
												  int source_channels,
												  int num_per_second,
												  int channel,
												  bool normalize) {
	AudioWaveformData data;
	if (source_fps.ToDouble() <= 0.0) {
		return data;
	}
	if (channel != -1 && (channel < 0 || channel >= source_channels)) {
		return data;
	}

	data = ApplyKeyframes(base.max_samples, base.rms_samples, time_keyframe, volume_keyframe, project_fps, num_per_second);

	if (normalize) {
		float samples_max = 0.0f;
		for (auto v : data.max_samples) {
			samples_max = std::max(samples_max, std::abs(v));
		}
		if (samples_max > 0.0f) {
			data.scale(static_cast<int>(data.max_samples.size()), 1.0f / samples_max);
		}
	}

	return data;
}

AudioWaveformData AudioWaveformer::ApplyKeyframes(const std::vector<float>& max_samples,
												  const std::vector<float>& rms_samples,
												  const Keyframe* time_keyframe,
												  const Keyframe* volume_keyframe,
												  const Fraction& project_fps,
												  int num_per_second) {
	AudioWaveformData data;
	if (num_per_second <= 0) {
		return data;
	}

	const double project_fps_value = project_fps.ToDouble();
	if (project_fps_value <= 0.0) {
		return data;
	}

	const size_t base_total = max_samples.size();
	if (base_total == 0) {
		return data;
	}
	const size_t rms_total = std::min(rms_samples.size(), base_total);

	// A single point cannot remap time, and an empty volume curve has no gain to apply
	if (time_keyframe && time_keyframe->GetCount() < 2) {
		time_keyframe = nullptr;
	}
	if (volume_keyframe && volume_keyframe->GetCount() < 1) {
		volume_keyframe = nullptr;
	}

	// Output duration follows the time curve (project frames), otherwise the source envelope
	int64_t total_samples = static_cast<int64_t>(base_total);
	if (time_keyframe) {
		const int64_t output_frames = time_keyframe->GetLength();
		if (output_frames <= 0) {
			return data;
		}
		const double output_duration_seconds = static_cast<double>(output_frames) / project_fps_value;
		total_samples = static_cast<int64_t>(std::ceil(output_duration_seconds * num_per_second));
	}
	if (total_samples <= 0) {
		return data;
	}

	data.max_samples.assign(static_cast<size_t>(total_samples), 0.0f);
	data.rms_samples.assign(static_cast<size_t>(total_samples), 0.0f);

	const double frames_per_sample = project_fps_value / static_cast<double>(num_per_second);
	const double samples_per_frame = static_cast<double>(num_per_second) / project_fps_value;

	for (int64_t i = 0; i < total_samples; ++i) {
		// 1-based project frame at this output sample (keyframes use 1-based frame numbers)
		const double project_frame = static_cast<double>(i) * frames_per_sample + 1.0;

		double source_index = static_cast<double>(i);
		if (time_keyframe) {
			const double mapped_frame = KeyframeValueAt(*time_keyframe, project_frame);
			source_index = (mapped_frame - 1.0) * samples_per_frame;
		}

		// Sample base waveform (nearest with simple linear blend)
		const int64_t idx0 = static_cast<int64_t>(std::floor(source_index));
		const int64_t idx1 = idx0 + 1;
		const double frac = source_index - static_cast<double>(idx0);

		float max_sample = 0.0f;
		float rms_sample = 0.0f;
		if (idx0 >= 0 && idx0 < static_cast<int64_t>(base_total)) {
			max_sample = max_samples[idx0];
			if (idx0 < static_cast<int64_t>(rms_total)) {
				rms_sample = rms_samples[idx0];
			}
		}
		if (frac > 0.0 && idx1 >= 0 && idx1 < static_cast<int64_t>(base_total)) {
			max_sample = static_cast<float>((1.0 - frac) * max_sample + frac * max_samples[idx1]);
			if (idx1 < static_cast<int64_t>(rms_total)) {
				rms_sample = static_cast<float>((1.0 - frac) * rms_sample + frac * rms_samples[idx1]);
			}
		}

		double gain = 1.0;
		if (volume_keyframe) {
			gain = std::abs(KeyframeValueAt(*volume_keyframe, project_frame));
		}

		data.max_samples[i] = static_cast<float>(max_sample * gain);
		data.rms_samples[i] = static_cast<float>(rms_sample * gain);
	}

	return data;
}

AudioWaveformData AudioWaveformer::ExtractSamplesFromReader(ReaderBase* source_reader, int channel, int num_per_second, bool normalize, bool per_channel) {
	AudioWaveformData data;

	if (!source_reader || num_per_second <= 0) {
		return data;
	}

	// Open reader (if needed)
	if (!source_reader->IsOpen()) {
		source_reader->Open();
		if (source_reader == detached_reader.get()) {
			LimitWaveformReaderCache(source_reader);
		}
	}

	const auto retry_delay = std::chrono::milliseconds(100);
	const auto max_wait_for_open = std::chrono::milliseconds(3000);

	auto get_frame_with_retry = [&](int64_t frame_number) -> std::shared_ptr<openshot::Frame> {
		std::chrono::steady_clock::time_point wait_start;
		bool waiting_for_open = false;
		while (true) {
			try {
				return source_reader->GetFrame(frame_number);
			} catch (const openshot::ReaderClosed&) {
				auto now = std::chrono::steady_clock::now();
				if (!waiting_for_open) {
					waiting_for_open = true;
					wait_start = now;
				} else if (now - wait_start >= max_wait_for_open) {
					throw;
				}

				std::this_thread::sleep_for(retry_delay);
			}
		}
	};

	int64_t sample_rate = source_reader->info.sample_rate;
	if (sample_rate <= 0) {
		sample_rate = num_per_second;
	}

	// Determine length of video frames (for waveform)
	int64_t reader_video_length = source_reader->info.video_length;
	if (reader_video_length < 0) {
		reader_video_length = 0;
	}
	float reader_duration = source_reader->info.duration;
	double fps_value = source_reader->info.fps.ToDouble();
	float frames_duration = 0.0f;
	if (reader_video_length > 0 && fps_value > 0.0) {
		frames_duration = static_cast<float>(reader_video_length / fps_value);
	}
	if (reader_duration <= 0.0f) {
		reader_duration = frames_duration;
	}
	if (reader_duration < 0.0f) {
		reader_duration = 0.0f;
	}

	if (!source_reader->info.has_audio) {
		return data;
	}

	const int reader_channels = source_reader->info.channels;
	if (reader_channels <= 0) {
		return data;
	}

	if (channel != -1 && (channel < 0 || channel >= reader_channels)) {
		return data;
	}

	// Per-channel envelopes describe every source channel, so they need all channels
	per_channel = per_channel && channel == -1;

	const bool known_duration = reader_duration > 0.0f && reader_video_length > 0;
	const int total_samples = known_duration ? static_cast<int>(std::ceil(reader_duration * num_per_second)) : 0;

	// Resize and clear audio buffers when duration is known. Some audio-only
	// formats, especially raw FLAC, may not expose a container duration even
	// though frames are readable, so those are appended dynamically below.
	if (known_duration) {
		data.resize(total_samples);
		data.zero(total_samples);
	} else {
		data.max_samples.reserve(num_per_second);
		data.rms_samples.reserve(num_per_second);
	}
	if (per_channel) {
		data.channel_max_samples.assign(reader_channels, std::vector<float>(known_duration ? total_samples : 0, 0.0f));
		data.channel_rms_samples.assign(reader_channels, std::vector<float>(known_duration ? total_samples : 0, 0.0f));
	}

	// Channels which contribute to the requested envelope
	std::vector<int> selected_channels;
	for (int channel_index = 0; channel_index < reader_channels; channel_index++) {
		if (channel == -1 || channel == channel_index) {
			selected_channels.push_back(channel_index);
		}
	}

	// Per-channel accumulators for the current chunk
	std::vector<float> chunk_max(reader_channels, 0.0f);
	std::vector<double> chunk_squared_sum(reader_channels, 0.0);
	std::vector<const float*> channel_samples(reader_channels, nullptr);

	// Chunk k covers source samples [boundary(k - 1), boundary(k)). Boundaries are rounded
	// from the exact rational position, so rates like 44100 / 200 = 220.5 never drift.
	auto chunk_boundary = [&](int64_t chunk_index) -> int64_t {
		return ((chunk_index + 1) * sample_rate + num_per_second / 2) / num_per_second;
	};

	int extracted_index = 0;
	int64_t samples_read = 0;
	int64_t chunk_sample_count = 0;
	int64_t next_boundary = std::max<int64_t>(1, chunk_boundary(0));
	float samples_max = 0.0f;

	const double frame_rate = fps_value > 0.0 ? fps_value : 30.0;
	const int64_t max_unknown_frames = static_cast<int64_t>(std::ceil(frame_rate * 60.0 * 60.0 * 12.0));
	int empty_audio_frames = 0;

	auto has_room = [&]() {
		return !known_duration || extracted_index < total_samples;
	};

	// Store the current chunk as one envelope sample, then reset the accumulators
	auto emit_chunk = [&]() {
		if (chunk_sample_count <= 0) {
			return;
		}
		if (has_room()) {
			float mixed_max = 0.0f;
			double mixed_squared_sum = 0.0;
			for (int channel_index : selected_channels) {
				mixed_max = std::max(mixed_max, chunk_max[channel_index]);
				mixed_squared_sum += chunk_squared_sum[channel_index];
			}
			const double mixed_count = static_cast<double>(chunk_sample_count) * static_cast<double>(selected_channels.size());
			const float mixed_rms = static_cast<float>(std::sqrt(mixed_squared_sum / mixed_count));

			if (known_duration) {
				data.max_samples[extracted_index] = mixed_max;
				data.rms_samples[extracted_index] = mixed_rms;
			} else {
				data.max_samples.push_back(mixed_max);
				data.rms_samples.push_back(mixed_rms);
			}
			if (per_channel) {
				for (int channel_index = 0; channel_index < reader_channels; channel_index++) {
					const float channel_rms = static_cast<float>(std::sqrt(
						chunk_squared_sum[channel_index] / static_cast<double>(chunk_sample_count)));
					if (known_duration) {
						data.channel_max_samples[channel_index][extracted_index] = chunk_max[channel_index];
						data.channel_rms_samples[channel_index][extracted_index] = channel_rms;
					} else {
						data.channel_max_samples[channel_index].push_back(chunk_max[channel_index]);
						data.channel_rms_samples[channel_index].push_back(channel_rms);
					}
				}
			}
			samples_max = std::max(samples_max, mixed_max);
			extracted_index++;
		}

		std::fill(chunk_max.begin(), chunk_max.end(), 0.0f);
		std::fill(chunk_squared_sum.begin(), chunk_squared_sum.end(), 0.0);
		chunk_sample_count = 0;
		next_boundary = std::max(samples_read + 1, chunk_boundary(extracted_index));
	};

	for (int64_t f = 1;
		 (known_duration ? f <= reader_video_length && extracted_index < total_samples : f <= max_unknown_frames);
		 f++) {
		std::shared_ptr<openshot::Frame> frame = get_frame_with_retry(f);
		if (!frame) {
			break;
		}
		if (!known_duration && frame->number < f) {
			break;
		}
		const int sample_count = frame->GetAudioSamplesCount();
		if (sample_count <= 0) {
			if (!known_duration && extracted_index > 0 && ++empty_audio_frames >= 3) {
				break;
			}
			continue;
		}
		empty_audio_frames = 0;

		const int frame_channels = frame->GetAudioChannelsCount();
		for (int channel_index : selected_channels) {
			channel_samples[channel_index] = channel_index < frame_channels ? frame->GetAudioSamples(channel_index) : nullptr;
		}

		int64_t offset = 0;
		while (offset < sample_count) {
			const int64_t span = std::min<int64_t>(sample_count - offset, next_boundary - samples_read);
			for (int channel_index : selected_channels) {
				const float* samples = channel_samples[channel_index];
				if (!samples) {
					continue;
				}
				float span_max = chunk_max[channel_index];
				double span_squared_sum = chunk_squared_sum[channel_index];
				for (int64_t s = offset; s < offset + span; s++) {
					const float value = samples[s];
					span_max = std::max(span_max, std::abs(value));
					span_squared_sum += static_cast<double>(value) * static_cast<double>(value);
				}
				chunk_max[channel_index] = span_max;
				chunk_squared_sum[channel_index] = span_squared_sum;
			}
			offset += span;
			samples_read += span;
			chunk_sample_count += span;

			if (samples_read >= next_boundary) {
				emit_chunk();
				if (!has_room()) {
					break;
				}
			}
		}
	}

	// Keep the trailing partial chunk (end of the media)
	emit_chunk();

	if (normalize && samples_max > 0.0f) {
		float scale = 1.0f / samples_max;
		data.scale(static_cast<int>(data.max_samples.size()), scale);
	}

	return data;
}

ReaderBase* AudioWaveformer::ResolveSourceReader(ReaderBase* source_reader) {
	if (!source_reader) {
		return nullptr;
	}

	ReaderBase* current = source_reader;
	while (true) {
		if (auto clip = dynamic_cast<Clip*>(current)) {
			current = clip->Reader();
			continue;
		}
		if (auto mapper = dynamic_cast<FrameMapper*>(current)) {
			current = mapper->Reader();
			continue;
		}
		break;
	}
	return current;
}

Fraction AudioWaveformer::ResolveSourceFPS(ReaderBase* source_reader) {
	if (!source_reader) {
		return Fraction(0, 1);
	}
	return source_reader->info.fps;
}

// Resolve and cache the reader used for waveform extraction (prefer a detached FFmpegReader clone)
ReaderBase* AudioWaveformer::ResolveWaveformReader() {
	if (source_initialized) {
		return resolved_reader ? resolved_reader : reader;
	}
	source_initialized = true;

	resolved_reader = ResolveSourceReader(reader);

	// Prefer a detached, audio-only FFmpegReader clone so we never mutate the live reader used for preview.
	if (auto ff_reader = dynamic_cast<FFmpegReader*>(resolved_reader)) {
		const Json::Value ff_json = ff_reader->JsonValue();
		const std::string path = ff_json.get("path", "").asString();
		if (!path.empty()) {
			try {
				auto clone = std::make_unique<FFmpegReader>(path, false);
				clone->SetJsonValue(ff_json);
				clone->info.has_video = false; // explicitly audio-only for waveform extraction
				detached_reader = std::move(clone);
				resolved_reader = detached_reader.get();
			} catch (...) {
				// Fall back to using the original reader if cloning fails
				detached_reader.reset();
				resolved_reader = ResolveSourceReader(reader);
			}
		}
	}

	return resolved_reader ? resolved_reader : reader;
}
