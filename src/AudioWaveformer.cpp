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

AudioWaveformData AudioWaveformer::ExtractSamples(const std::string& path, int channel, int num_per_second, bool normalize) {
	FFmpegReader temp_reader(path);
	temp_reader.Open();
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
	if (num_per_second <= 0) {
		return data;
	}

	double project_fps_value = project_fps.ToDouble();
	double source_fps_value = source_fps.ToDouble();
	if (project_fps_value <= 0.0 || source_fps_value <= 0.0) {
		return data;
	}

	if (channel != -1 && (channel < 0 || channel >= source_channels)) {
		return data;
	}

	size_t base_total = base.max_samples.size();
	if (base_total == 0) {
		return data;
	}

	// Determine output duration from time curve (if any). Time curves are in project-frame domain.
	int64_t output_frames = 0;
	if (time_keyframe && time_keyframe->GetCount() > 0) {
		output_frames = time_keyframe->GetLength();
	}
	if (output_frames <= 0) {
		// Default to source duration derived from base waveform length
		double source_duration = static_cast<double>(base_total) / static_cast<double>(num_per_second);
		output_frames = static_cast<int64_t>(std::llround(source_duration * project_fps_value));
	}
	double output_duration_seconds = static_cast<double>(output_frames) / project_fps_value;
	int total_samples = static_cast<int>(std::ceil(output_duration_seconds * num_per_second));

	if (total_samples <= 0) {
		return data;
	}

	data.resize(total_samples);
	data.zero(total_samples);

	for (int i = 0; i < total_samples; ++i) {
		double out_time = static_cast<double>(i) / static_cast<double>(num_per_second);
		// Time keyframes are defined in project-frame domain; evaluate using project frames
		double project_frame = out_time * project_fps_value;
		double mapped_project_frame = time_keyframe ? time_keyframe->GetValue(project_frame) : project_frame;
		// Convert mapped project frame to seconds (project FPS), then to waveform index
		double source_time = mapped_project_frame / project_fps_value;
		double source_index = source_time * static_cast<double>(num_per_second);

		// Sample base waveform (nearest with simple linear blend)
		int idx0 = static_cast<int>(std::floor(source_index));
		int idx1 = idx0 + 1;
		double frac = source_index - static_cast<double>(idx0);

		float max_sample = 0.0f;
		float rms_sample = 0.0f;
		if (idx0 >= 0 && idx0 < static_cast<int>(base_total)) {
			max_sample = base.max_samples[idx0];
			rms_sample = base.rms_samples[idx0];
		}
		if (idx1 >= 0 && idx1 < static_cast<int>(base_total)) {
			max_sample = static_cast<float>((1.0 - frac) * max_sample + frac * base.max_samples[idx1]);
			rms_sample = static_cast<float>((1.0 - frac) * rms_sample + frac * base.rms_samples[idx1]);
		}

		double gain = 1.0;
		if (volume_keyframe) {
			double project_frame = out_time * project_fps_value;
			gain = volume_keyframe->GetValue(project_frame);
		}
		max_sample = static_cast<float>(max_sample * gain);
		rms_sample = static_cast<float>(rms_sample * gain);

		data.max_samples[i] = max_sample;
		data.rms_samples[i] = rms_sample;
	}

	if (normalize) {
		float samples_max = 0.0f;
		for (auto v : data.max_samples) {
			samples_max = std::max(samples_max, std::abs(v));
		}
		if (samples_max > 0.0f) {
			data.scale(total_samples, 1.0f / samples_max);
		}
	}

	return data;
}

AudioWaveformData AudioWaveformer::ExtractSamplesFromReader(ReaderBase* source_reader, int channel, int num_per_second, bool normalize) {
	AudioWaveformData data;

	if (!source_reader || num_per_second <= 0) {
		return data;
	}

	// Open reader (if needed)
	if (!source_reader->IsOpen()) {
		source_reader->Open();
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

	int sample_rate = source_reader->info.sample_rate;
	if (sample_rate <= 0) {
		sample_rate = num_per_second;
	}
	int sample_divisor = sample_rate / num_per_second;
	if (sample_divisor <= 0) {
		sample_divisor = 1;
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

	int total_samples = static_cast<int>(std::ceil(reader_duration * num_per_second));
	if (total_samples <= 0 || source_reader->info.channels == 0) {
		return data;
	}

	if (channel != -1 && (channel < 0 || channel >= source_reader->info.channels)) {
		return data;
	}

	// Resize and clear audio buffers
	data.resize(total_samples);
	data.zero(total_samples);

	int extracted_index = 0;
	int sample_index = 0;
	float samples_max = 0.0f;
	float chunk_max = 0.0f;
	double chunk_squared_sum = 0.0;

	int channel_count = (channel == -1) ? source_reader->info.channels : 1;
	std::vector<float*> channels(source_reader->info.channels, nullptr);

	try {
		for (int64_t f = 1; f <= reader_video_length && extracted_index < total_samples; f++) {
			std::shared_ptr<openshot::Frame> frame = get_frame_with_retry(f);

			for (int channel_index = 0; channel_index < source_reader->info.channels; channel_index++) {
				if (channel == channel_index || channel == -1) {
					channels[channel_index] = frame->GetAudioSamples(channel_index);
				}
			}

			int sample_count = frame->GetAudioSamplesCount();
			for (int s = 0; s < sample_count; s++) {
				for (int channel_index = 0; channel_index < source_reader->info.channels; channel_index++) {
					if (channel == channel_index || channel == -1) {
						float *samples = channels[channel_index];
						if (!samples) {
							continue;
						}
						float abs_sample = std::abs(samples[s]);
						chunk_squared_sum += static_cast<double>(samples[s]) * static_cast<double>(samples[s]);
						chunk_max = std::max(chunk_max, abs_sample);
					}
				}

				sample_index += 1;

				if (sample_index % sample_divisor == 0) {
					float avg_squared_sum = 0.0f;
					if (channel_count > 0) {
						avg_squared_sum = static_cast<float>(chunk_squared_sum / static_cast<double>(sample_divisor * channel_count));
					}

					if (extracted_index < total_samples) {
						data.max_samples[extracted_index] = chunk_max;
						data.rms_samples[extracted_index] = std::sqrt(avg_squared_sum);
						samples_max = std::max(samples_max, chunk_max);
						extracted_index++;
					}

					sample_index = 0;
					chunk_max = 0.0f;
					chunk_squared_sum = 0.0;

					if (extracted_index >= total_samples) {
						break;
					}
				}
			}
		}
	} catch (...) {
		throw;
	}

	if (sample_index > 0 && extracted_index < total_samples) {
		float avg_squared_sum = 0.0f;
		if (channel_count > 0) {
			avg_squared_sum = static_cast<float>(chunk_squared_sum / static_cast<double>(sample_index * channel_count));
		}

		data.max_samples[extracted_index] = chunk_max;
		data.rms_samples[extracted_index] = std::sqrt(avg_squared_sum);
		samples_max = std::max(samples_max, chunk_max);
		extracted_index++;
	}

	if (normalize && samples_max > 0.0f) {
		float scale = 1.0f / samples_max;
		data.scale(total_samples, scale);
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
