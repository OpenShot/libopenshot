/**
 * @file
 * @brief Source file for audio recording classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioRecorder.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Exceptions.h"
#include "FFmpegWriter.h"
#include "Frame.h"

using namespace openshot;

AudioLevelData AudioRecorderLevelMeter::ProcessBlock(const AudioRecorderBlock& block) const
{
	AudioLevelData result;
	result.timestamp = block.sample_rate > 0
		? static_cast<double>(block.first_sample) / static_cast<double>(block.sample_rate)
		: 0.0;

	const int channels = static_cast<int>(block.channels.size());
	result.peak.assign(channels, 0.0f);
	result.rms.assign(channels, 0.0f);

	for (int channel = 0; channel < channels; ++channel) {
		const auto& samples = block.channels[channel];
		double squared_sum = 0.0;
		float peak = 0.0f;

		for (float sample : samples) {
			const float abs_sample = std::abs(sample);
			peak = std::max(peak, abs_sample);
			squared_sum += static_cast<double>(sample) * static_cast<double>(sample);
			if (abs_sample >= 1.0f) {
				result.clipped = true;
			}
		}

		result.peak[channel] = peak;
		if (!samples.empty()) {
			result.rms[channel] = static_cast<float>(std::sqrt(squared_sum / static_cast<double>(samples.size())));
		}
	}

	return result;
}

AudioRecorderWaveformAccumulator::AudioRecorderWaveformAccumulator(int new_sample_rate, int new_samples_per_second)
	: sample_rate(new_sample_rate)
	, samples_per_second(new_samples_per_second)
	, sample_divisor(1)
	, pending_samples(0)
	, pending_max(0.0f)
	, pending_squared_sum(0.0)
	, emitted_visual_samples(0)
{
	if (sample_rate <= 0 || samples_per_second <= 0) {
		throw InvalidOptions("Audio waveform settings require a valid sample rate and samples-per-second value.");
	}

	sample_divisor = std::max(1, sample_rate / samples_per_second);
}

std::vector<AudioWaveformChunk> AudioRecorderWaveformAccumulator::ProcessBlock(const AudioRecorderBlock& block)
{
	std::vector<AudioWaveformChunk> chunks;
	if (block.channels.empty() || block.Samples() <= 0) {
		return chunks;
	}

	AudioWaveformChunk chunk;
	chunk.samples_per_second = samples_per_second;
	chunk.start_time = static_cast<double>(emitted_visual_samples) / static_cast<double>(samples_per_second);

	const int channels = static_cast<int>(block.channels.size());
	const int samples = block.Samples();
	for (int sample_index = 0; sample_index < samples; ++sample_index) {
		for (int channel = 0; channel < channels; ++channel) {
			if (sample_index >= static_cast<int>(block.channels[channel].size())) {
				continue;
			}
			const float sample = block.channels[channel][sample_index];
			pending_max = std::max(pending_max, std::abs(sample));
			pending_squared_sum += static_cast<double>(sample) * static_cast<double>(sample);
		}

		pending_samples++;
		if (pending_samples >= sample_divisor) {
			const double denominator = static_cast<double>(pending_samples * channels);
			const float rms = denominator > 0.0
				? static_cast<float>(std::sqrt(pending_squared_sum / denominator))
				: 0.0f;

			max_samples.push_back(pending_max);
			rms_samples.push_back(rms);
			chunk.max_samples.push_back(pending_max);
			chunk.rms_samples.push_back(rms);
			emitted_visual_samples++;

			pending_samples = 0;
			pending_max = 0.0f;
			pending_squared_sum = 0.0;
		}
	}

	if (!chunk.max_samples.empty()) {
		chunk.duration = static_cast<double>(chunk.max_samples.size()) / static_cast<double>(samples_per_second);
		chunks.push_back(std::move(chunk));
	}

	return chunks;
}

AudioWaveformData AudioRecorderWaveformAccumulator::Snapshot() const
{
	AudioWaveformData result;
	result.max_samples = max_samples;
	result.rms_samples = rms_samples;
	return result;
}

void AudioRecorderWaveformAccumulator::Reset()
{
	pending_samples = 0;
	pending_max = 0.0f;
	pending_squared_sum = 0.0;
	emitted_visual_samples = 0;
	max_samples.clear();
	rms_samples.clear();
}

std::shared_ptr<Frame> AudioRecordingFrameFactory::CreateFrame(
	const AudioRecorderBlock& block,
	ChannelLayout channel_layout,
	int64_t frame_number)
{
	const int channels = static_cast<int>(block.channels.size());
	const int samples = block.Samples();
	auto frame = std::make_shared<Frame>(frame_number, samples, channels);
	frame->SampleRate(block.sample_rate);
	frame->ChannelsLayout(channel_layout);

	for (int channel = 0; channel < channels; ++channel) {
		frame->AddAudio(true, channel, 0, block.channels[channel].data(), samples, 1.0f);
	}

	return frame;
}

AudioRecorder::AudioRecorder(const AudioRecorderSettings& new_settings)
	: settings(new_settings)
	, writer(nullptr)
	, waveform_accumulator(nullptr)
	, is_open(false)
	, is_recording(false)
	, is_monitoring(false)
	, writer_should_stop(false)
	, samples_recorded(0)
	, dropped_blocks(0)
	, next_frame_number(1)
{
	ValidateSettings();
}

AudioRecorder::~AudioRecorder()
{
	Close();
}

void AudioRecorder::ValidateSettings() const
{
	if (settings.path.empty()) {
		throw InvalidOptions("Audio recorder requires an output path.");
	}
	if (settings.codec.empty()) {
		throw InvalidOptions("Audio recorder requires an audio codec.");
	}
	if (settings.sample_rate < 8000) {
		throw InvalidSampleRate("Audio recorder requires a sample rate of at least 8000 Hz.", settings.path);
	}
	if (settings.channels <= 0) {
		throw InvalidChannels("Audio recorder requires at least one input channel.", settings.path);
	}
	if (settings.buffer_size <= 0) {
		throw InvalidOptions("Audio recorder requires a positive audio buffer size.", settings.path);
	}
	if (settings.waveform_samples_per_second <= 0) {
		throw InvalidOptions("Audio recorder requires a positive waveform sample rate.", settings.path);
	}
	if (settings.max_queue_seconds <= 0) {
		throw InvalidOptions("Audio recorder requires a positive maximum queue duration.", settings.path);
	}
}

void AudioRecorder::Open()
{
	if (is_open) {
		return;
	}

	if (!settings.device_type.empty()) {
		// Device types are created lazily. Selecting a type before the scan is
		// a no-op, which can send a DirectSound input name to WASAPI on Windows.
		device_manager.getAvailableDeviceTypes();
		device_manager.setCurrentAudioDeviceType(settings.device_type, true);
	}

	juce::AudioDeviceManager::AudioDeviceSetup setup;
	setup.inputChannels.clear();
	for (int channel = 0; channel < settings.channels; ++channel) {
		setup.inputChannels.setBit(channel);
	}
	setup.outputChannels.clear();
	setup.sampleRate = settings.sample_rate;
	setup.bufferSize = settings.buffer_size;
	setup.inputDeviceName = settings.device_name;

	const juce::String error = device_manager.initialise(
		settings.channels,
		0,
		nullptr,
		true,
		settings.device_name,
		&setup);
	if (error.isNotEmpty()) {
		throw InvalidOptions(error.toStdString(), settings.path);
	}

	if (auto* device = device_manager.getCurrentAudioDevice()) {
		const double actual_rate = device->getCurrentSampleRate();
		if (actual_rate > 0.0) {
			settings.sample_rate = static_cast<int>(std::llround(actual_rate));
		}
	}

	waveform_accumulator = std::make_unique<AudioRecorderWaveformAccumulator>(
		settings.sample_rate,
		settings.waveform_samples_per_second);

	is_open = true;
}

void AudioRecorder::OpenWriter()
{
	if (writer) {
		return;
	}

	writer = std::make_unique<FFmpegWriter>(settings.path);
	writer->SetAudioOptions(true, settings.codec, settings.sample_rate, settings.channels, settings.channel_layout, settings.bit_rate);
	writer->Open();
}

void AudioRecorder::Start()
{
	if (is_recording) {
		return;
	}

	PrepareRecording();

	writer_should_stop = false;
	is_recording = true;
	device_manager.addAudioCallback(this);
	writer_thread = std::thread(&AudioRecorder::WriterLoop, this);
}

void AudioRecorder::PrepareRecording()
{
	if (!is_open) {
		Open();
	}
	StopMonitoring();
	OpenWriter();
	if (waveform_accumulator) {
		waveform_accumulator->Reset();
	}
	samples_recorded = 0;
	dropped_blocks = 0;
	next_frame_number = 1;
}

void AudioRecorder::Stop()
{
	if (!is_recording && !writer_thread.joinable()) {
		if (writer) {
			writer->Close();
			writer.reset();
		}
		return;
	}

	is_recording = false;
	device_manager.removeAudioCallback(this);
	writer_should_stop = true;
	queue_condition.notify_all();

	if (writer_thread.joinable()) {
		writer_thread.join();
	}

	if (writer) {
		writer->Close();
		writer.reset();
	}
}

void AudioRecorder::StartMonitoring()
{
	if (is_recording || is_monitoring) {
		return;
	}

	if (!is_open) {
		Open();
	}

	is_monitoring = true;
	device_manager.addAudioCallback(this);
}

void AudioRecorder::StopMonitoring()
{
	if (!is_monitoring) {
		return;
	}

	is_monitoring = false;
	device_manager.removeAudioCallback(this);
}

void AudioRecorder::Close()
{
	StopMonitoring();
	Stop();

	if (is_open) {
		device_manager.closeAudioDevice();
		if (writer) {
			writer->Close();
			writer.reset();
		}
		is_open = false;
	}
}

bool AudioRecorder::IsOpen() const
{
	return is_open;
}

bool AudioRecorder::IsRecording() const
{
	return is_recording;
}

bool AudioRecorder::IsMonitoring() const
{
	return is_monitoring;
}

AudioRecorderStats AudioRecorder::GetStats() const
{
	AudioRecorderStats stats;
	stats.is_open = is_open;
	stats.is_recording = is_recording;
	stats.sample_rate = settings.sample_rate;
	stats.channels = settings.channels;
	stats.samples_recorded = samples_recorded;
	stats.dropped_blocks = dropped_blocks;
	stats.duration = settings.sample_rate > 0
		? static_cast<double>(stats.samples_recorded) / static_cast<double>(settings.sample_rate)
		: 0.0;

	std::lock_guard<std::mutex> lock(queue_mutex);
	stats.queued_blocks = static_cast<int64_t>(queue.size());
	return stats;
}

AudioWaveformData AudioRecorder::GetWaveformSnapshot() const
{
	std::lock_guard<std::mutex> lock(state_mutex);
	return waveform_accumulator ? waveform_accumulator->Snapshot() : AudioWaveformData();
}

AudioLevelData AudioRecorder::GetLevelSnapshot() const
{
	std::lock_guard<std::mutex> lock(state_mutex);
	return last_level;
}

void AudioRecorder::SetLevelCallback(std::function<void(const AudioLevelData&)> callback)
{
	std::lock_guard<std::mutex> lock(state_mutex);
	level_callback = std::move(callback);
}

void AudioRecorder::SetWaveformCallback(std::function<void(const AudioWaveformChunk&)> callback)
{
	std::lock_guard<std::mutex> lock(state_mutex);
	waveform_callback = std::move(callback);
}

void AudioRecorder::audioDeviceIOCallbackWithContext(
	const float* const* inputChannelData,
	int numInputChannels,
	float* const* outputChannelData,
	int numOutputChannels,
	int numSamples,
	const juce::AudioIODeviceCallbackContext&)
{
	for (int channel = 0; channel < numOutputChannels; ++channel) {
		if (outputChannelData[channel]) {
			std::fill(outputChannelData[channel], outputChannelData[channel] + numSamples, 0.0f);
		}
	}

	if ((!is_recording && !is_monitoring) || numSamples <= 0) {
		return;
	}

	AudioRecorderBlock block;
	block.sample_rate = settings.sample_rate;
	block.first_sample = samples_recorded.load();
	block.channels.resize(settings.channels);

	for (int channel = 0; channel < settings.channels; ++channel) {
		block.channels[channel].assign(numSamples, 0.0f);
		if (channel < numInputChannels && inputChannelData[channel]) {
			std::copy(inputChannelData[channel], inputChannelData[channel] + numSamples, block.channels[channel].begin());
		}
	}

	AudioLevelData level = level_meter.ProcessBlock(block);
	std::function<void(const AudioLevelData&)> level_cb;
	{
		std::lock_guard<std::mutex> lock(state_mutex);
		last_level = level;
		level_cb = level_callback;
	}
	if (level_cb) {
		level_cb(level);
	}

	if (!is_recording) {
		samples_recorded += numSamples;
		return;
	}

	const int64_t max_queue_blocks = static_cast<int64_t>(
		std::max(1, (settings.max_queue_seconds * settings.sample_rate) / std::max(1, numSamples)));
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (static_cast<int64_t>(queue.size()) >= max_queue_blocks) {
			dropped_blocks++;
		} else {
			queue.push_back(std::move(block));
			queue_condition.notify_one();
		}
	}

	samples_recorded += numSamples;
}

void AudioRecorder::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
	(void) device;
}

void AudioRecorder::audioDeviceStopped()
{
}

bool AudioRecorder::PopBlock(AudioRecorderBlock& block)
{
	std::unique_lock<std::mutex> lock(queue_mutex);
	queue_condition.wait(lock, [this]() {
		return writer_should_stop || !queue.empty();
	});

	if (queue.empty()) {
		return false;
	}

	block = std::move(queue.front());
	queue.pop_front();
	return true;
}

void AudioRecorder::WriterLoop()
{
	int64_t expected_next_sample = -1;
	while (true) {
		AudioRecorderBlock block;
		if (!PopBlock(block)) {
			if (writer_should_stop) {
				break;
			}
			continue;
		}

		std::vector<AudioWaveformChunk> waveform_chunks;
		std::function<void(const AudioWaveformChunk&)> waveform_cb;
		{
			std::lock_guard<std::mutex> lock(state_mutex);
			if (waveform_accumulator) {
				waveform_chunks = waveform_accumulator->ProcessBlock(block);
			}
			waveform_cb = waveform_callback;
		}

		if (waveform_cb) {
			for (const auto& chunk : waveform_chunks) {
				waveform_cb(chunk);
			}
		}

		if (writer) {
			while (expected_next_sample >= 0 && block.first_sample > expected_next_sample) {
				const int64_t missing_samples = block.first_sample - expected_next_sample;
				const int silence_samples = static_cast<int>(std::min<int64_t>(
					missing_samples,
					std::max(1, settings.buffer_size)));
				AudioRecorderBlock silence_block;
				silence_block.sample_rate = block.sample_rate;
				silence_block.first_sample = expected_next_sample;
				silence_block.channels.assign(
					settings.channels,
					std::vector<float>(silence_samples, 0.0f));
				writer->WriteFrame(AudioRecordingFrameFactory::CreateFrame(
					silence_block,
					settings.channel_layout,
					next_frame_number++));
				expected_next_sample += silence_samples;
			}

			writer->WriteFrame(AudioRecordingFrameFactory::CreateFrame(
				block,
				settings.channel_layout,
				next_frame_number++));
		}
		expected_next_sample = block.first_sample + block.Samples();
	}
}
