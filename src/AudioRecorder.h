/**
 * @file
 * @brief Header file for audio recording classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIORECORDER_H
#define OPENSHOT_AUDIORECORDER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "AudioWaveformer.h"
#include "ChannelLayouts.h"

namespace openshot
{
	class FFmpegWriter;
	class Frame;

	struct AudioRecorderSettings
	{
		std::string path;
		std::string device_name;
		std::string device_type;
		std::string codec = "pcm_s16le";
		int sample_rate = 48000;
		int channels = 1;
		openshot::ChannelLayout channel_layout = openshot::LAYOUT_MONO;
		int bit_rate = 192000;
		int buffer_size = 512;
		int waveform_samples_per_second = 30;
		int max_queue_seconds = 10;
		std::map<std::string, std::string> options;
	};

	struct AudioLevelData
	{
		double timestamp = 0.0;
		std::vector<float> peak;
		std::vector<float> rms;
		bool clipped = false;

		std::vector<std::vector<float>> vectors() const
		{
			std::vector<std::vector<float>> output;
			output.push_back(peak);
			output.push_back(rms);
			return output;
		}
	};

	struct AudioWaveformChunk
	{
		double start_time = 0.0;
		double duration = 0.0;
		int samples_per_second = 0;
		std::vector<float> max_samples;
		std::vector<float> rms_samples;

		std::vector<std::vector<float>> vectors() const
		{
			std::vector<std::vector<float>> output;
			output.push_back(max_samples);
			output.push_back(rms_samples);
			return output;
		}
	};

	struct AudioRecorderStats
	{
		bool is_open = false;
		bool is_recording = false;
		int sample_rate = 0;
		int channels = 0;
		int64_t samples_recorded = 0;
		int64_t dropped_blocks = 0;
		int64_t queued_blocks = 0;
		double duration = 0.0;
	};

	struct AudioRecorderBlock
	{
		int sample_rate = 0;
		int64_t first_sample = 0;
		std::vector<std::vector<float>> channels;

		int Samples() const
		{
			return channels.empty() ? 0 : static_cast<int>(channels.front().size());
		}
	};

	class AudioRecorderLevelMeter
	{
	public:
		AudioLevelData ProcessBlock(const AudioRecorderBlock& block) const;
	};

	class AudioRecorderWaveformAccumulator
	{
	public:
		AudioRecorderWaveformAccumulator(int sample_rate, int samples_per_second);

		std::vector<AudioWaveformChunk> ProcessBlock(const AudioRecorderBlock& block);
		openshot::AudioWaveformData Snapshot() const;
		void Reset();

	private:
		int sample_rate;
		int samples_per_second;
		int sample_divisor;
		int pending_samples;
		float pending_max;
		double pending_squared_sum;
		int64_t emitted_visual_samples;
		std::vector<float> max_samples;
		std::vector<float> rms_samples;
	};

	class AudioRecordingFrameFactory
	{
	public:
		static std::shared_ptr<openshot::Frame> CreateFrame(
			const AudioRecorderBlock& block,
			openshot::ChannelLayout channel_layout,
			int64_t frame_number);
	};

	class AudioRecorder
#ifndef SWIG
		: private juce::AudioIODeviceCallback
#endif
	{
	public:
		explicit AudioRecorder(const AudioRecorderSettings& settings);
		~AudioRecorder()
#ifndef SWIG
			override
#endif
			;

		void Open();
		void PrepareRecording();
		void Start();
		void Stop();
		void StartMonitoring();
		void StopMonitoring();
		void Close();

		bool IsOpen() const;
		bool IsRecording() const;
		bool IsMonitoring() const;
		AudioRecorderStats GetStats() const;
		openshot::AudioWaveformData GetWaveformSnapshot() const;
		AudioLevelData GetLevelSnapshot() const;

#ifndef SWIG
		void SetLevelCallback(std::function<void(const AudioLevelData&)> callback);
		void SetWaveformCallback(std::function<void(const AudioWaveformChunk&)> callback);
		void audioDeviceIOCallbackWithContext(
			const float* const* inputChannelData,
			int numInputChannels,
			float* const* outputChannelData,
			int numOutputChannels,
			int numSamples,
			const juce::AudioIODeviceCallbackContext& context) override;
		void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
		void audioDeviceStopped() override;
#endif

	private:
		void ValidateSettings() const;
		void OpenWriter();
		void WriterLoop();
		bool PopBlock(AudioRecorderBlock& block);

		AudioRecorderSettings settings;
		juce::AudioDeviceManager device_manager;
		std::unique_ptr<openshot::FFmpegWriter> writer;
		std::unique_ptr<AudioRecorderWaveformAccumulator> waveform_accumulator;
		AudioRecorderLevelMeter level_meter;

		mutable std::mutex state_mutex;
		mutable std::mutex queue_mutex;
		std::condition_variable queue_condition;
		std::deque<AudioRecorderBlock> queue;
		std::thread writer_thread;

		std::function<void(const AudioLevelData&)> level_callback;
		std::function<void(const AudioWaveformChunk&)> waveform_callback;
		AudioLevelData last_level;

		std::atomic<bool> is_open;
		std::atomic<bool> is_recording;
		std::atomic<bool> is_monitoring;
		std::atomic<bool> writer_should_stop;
		std::atomic<int64_t> samples_recorded;
		std::atomic<int64_t> dropped_blocks;
		int64_t next_frame_number;
	};
}

#endif
