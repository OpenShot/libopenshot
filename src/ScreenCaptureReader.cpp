/**
 * @file
 * @brief Source file for live FFmpeg screen capture readers
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ScreenCaptureReader.h"
#include "CaptureAudioBuffer.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#if defined(__linux__)
	#include <dlfcn.h>
#endif

extern "C" {
	#include <libavdevice/avdevice.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/time.h>
}

#if defined(_WIN32)
	// Instantiate the Core Audio COM GUIDs in this translation unit. Older
	// MinGW-w64 uuid import libraries do not provide all WASAPI identifiers.
	#include <initguid.h>
	#include <audioclient.h>
	#include <mmdeviceapi.h>
#endif

#include "Exceptions.h"
#include "Frame.h"
#include "QtUtilities.h"

using namespace openshot;

#if defined(HAVE_WAYLAND_CAPTURE_PLUGIN) && defined(__linux__)
namespace
{
	using WaylandBackendFactory = std::unique_ptr<ScreenCaptureReader::CaptureBackendReader> (*) (
		const ScreenCaptureSettings&, ReaderInfo&);

	void* load_wayland_capture_backend(std::string& error_message)
	{
		Dl_info library_info {};
		if (dladdr(reinterpret_cast<void*>(load_wayland_capture_backend), &library_info)
			&& library_info.dli_fname) {
			std::string library_path(library_info.dli_fname);
			const auto separator = library_path.find_last_of('/');
			if (separator != std::string::npos) {
				const std::string module_path = library_path.substr(0, separator + 1)
					+ "libopenshot-wayland-capture.so";
				if (void* module = dlopen(module_path.c_str(), RTLD_NOW | RTLD_LOCAL)) {
					return module;
				}
				const char* error = dlerror();
				error_message = error ? error : "unable to load PipeWire support.";
				return nullptr;
			}
		}
		void* module = dlopen("libopenshot-wayland-capture.so", RTLD_NOW | RTLD_LOCAL);
		if (!module) {
			const char* error = dlerror();
			error_message = error ? error : "unable to load PipeWire support.";
		}
		return module;
	}
}
#endif

#if defined(__linux__)
class ScreenCaptureReader::SystemAudioCapture
{
public:
	explicit SystemAudioCapture(const ScreenCaptureSettings& new_settings)
		: settings(new_settings) {}

	~SystemAudioCapture() { Close(); }

	void Open()
	{
		if (format_context) return;
		avdevice_register_all();
		AVInputFormat* input_format = const_cast<AVInputFormat*>(av_find_input_format("pulse"));
		if (!input_format) {
			throw InvalidOptions("FFmpeg PulseAudio input is unavailable for system audio capture.");
		}

		format_context = avformat_alloc_context();
		if (!format_context) {
			throw OutOfMemory("Unable to allocate system audio capture context.");
		}
		format_context->interrupt_callback.callback = InterruptCallback;
		format_context->interrupt_callback.opaque = &close_requested;

		AVDictionary* options = nullptr;
		av_dict_set(&options, "wallclock", "1", 0);
		// Pulse defaults to server-selected buffering, which can deliver large
		// batches too late for a live video frame. Request 20 ms of default
		// 16-bit PCM; timestamps still determine placement, not this buffer size.
		av_dict_set_int(&options, "fragment_size",
			static_cast<int64_t>(settings.audio_sample_rate) * settings.audio_channels * 2 / 50, 0);
		av_dict_set(&options, "sample_rate", std::to_string(settings.audio_sample_rate).c_str(), 0);
		av_dict_set(&options, "channels", std::to_string(settings.audio_channels).c_str(), 0);
		const std::string device = settings.audio_device.empty() ? "@DEFAULT_MONITOR@" : settings.audio_device;
		const int open_result = avformat_open_input(&format_context, device.c_str(), input_format, &options);
		av_dict_free(&options);
		if (open_result < 0) {
			throw InvalidFile("Unable to open system audio capture input: " + std::string(av_err2str(open_result)), device);
		}
		if (avformat_find_stream_info(format_context, nullptr) < 0) {
			throw InvalidFile("Unable to read system audio capture stream information.", device);
		}
		for (unsigned int index = 0; index < format_context->nb_streams; ++index) {
			if (format_context->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
				audio_stream = static_cast<int>(index);
				break;
			}
		}
		if (audio_stream < 0) {
			throw InvalidFile("No audio stream found in system audio capture input.", device);
		}
		AVStream* stream = format_context->streams[audio_stream];
		const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
		codec_context = codec ? ffmpeg_get_codec_context(stream, codec) : nullptr;
		if (codec_context) codec_context->pkt_timebase = stream->time_base;
		if (!codec_context || avcodec_open2(codec_context, codec, nullptr) < 0) {
			throw InvalidCodec("Unable to open system audio capture decoder.", device);
		}
		packet = av_packet_alloc();
		decoded_frame = av_frame_alloc();
		if (!packet || !decoded_frame) {
			throw OutOfMemory("Unable to allocate system audio capture buffers.");
		}
		settings.audio_sample_rate = codec_context->sample_rate > 0
			? codec_context->sample_rate : settings.audio_sample_rate;
		Reset();
		close_requested = false;
		worker = std::thread(&SystemAudioCapture::CaptureLoop, this);
	}

	void Close()
	{
		close_requested = true;
		ready.notify_all();
		if (worker.joinable()) {
			worker.join();
		}
		if (decoded_frame) av_frame_free(&decoded_frame);
		if (packet) av_packet_free(&packet);
		if (codec_context) avcodec_free_context(&codec_context);
		if (format_context) avformat_close_input(&format_context);
	}

	void AddFrameAudio(const std::shared_ptr<Frame>& frame, int64_t number, const Fraction& fps)
	{
		int sample_count = 0;
		for (int64_t frame_number = last_output_frame + 1; frame_number <= number; ++frame_number) {
			sample_count += Frame::GetSamplesPerFrame(
				frame_number, fps, settings.audio_sample_rate, settings.audio_channels);
		}
		last_output_frame = std::max(last_output_frame, number);
		if (!frame || sample_count <= 0) return;

		std::unique_lock<std::mutex> lock(queue_mutex);
		// Wait for capture delivery, but keep its original sample positions. A
		// timed-out interval becomes silence; late packets cannot move it forward.
		const auto wait_time = timeline_started
			? std::chrono::milliseconds(100)
			: std::chrono::milliseconds(3000);
		ready.wait_for(lock, wait_time, [this, sample_count]() {
			return close_requested || audio_buffer.Covers(sample_count);
		});
		timeline_started = true;
		auto samples = audio_buffer.Read(sample_count);
		frame->SampleRate(settings.audio_sample_rate);
		frame->ChannelsLayout(settings.audio_channels == 1 ? LAYOUT_MONO : LAYOUT_STEREO);
		for (int channel = 0; channel < settings.audio_channels; ++channel) {
			frame->AddAudio(true, channel, 0, samples[channel].data(), sample_count, 1.0f);
		}
	}

	void Reset()
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		// PulseAudio wallclock PTS uses av_gettime(), corrected for input latency.
		// Reset the epoch too, so packets buffered before recording are discarded.
		epoch_us = av_gettime();
		audio_buffer.Reset(settings.audio_channels, static_cast<int64_t>(settings.audio_sample_rate) * 10);
		last_output_frame = 0;
		timeline_started = false;
	}

	int SampleRate() const { return settings.audio_sample_rate; }
	int Channels() const { return settings.audio_channels; }

private:
	static int InterruptCallback(void* opaque)
	{
		const auto* requested = static_cast<std::atomic<bool>*>(opaque);
		return requested && requested->load() ? 1 : 0;
	}

	float SampleAt(const AVFrame* frame, int channel, int sample) const
	{
		const AVSampleFormat format = static_cast<AVSampleFormat>(frame->format);
		const bool planar = av_sample_fmt_is_planar(format) != 0;
		const int source_channels = std::max(1,
#if HAVE_CH_LAYOUT
			codec_context->ch_layout.nb_channels
#else
			codec_context->channels
#endif
		);
		const int source_channel = std::min(channel, source_channels - 1);
		const uint8_t* data = planar ? frame->extended_data[source_channel] : frame->extended_data[0];
		const int offset = planar ? sample : sample * source_channels + source_channel;
		switch (format) {
		case AV_SAMPLE_FMT_U8: case AV_SAMPLE_FMT_U8P:
			return (static_cast<const uint8_t*>(static_cast<const void*>(data))[offset] - 128) / 128.0f;
		case AV_SAMPLE_FMT_S16: case AV_SAMPLE_FMT_S16P:
			return static_cast<const int16_t*>(static_cast<const void*>(data))[offset] / 32768.0f;
		case AV_SAMPLE_FMT_S32: case AV_SAMPLE_FMT_S32P:
			return static_cast<float>(static_cast<const int32_t*>(static_cast<const void*>(data))[offset] / 2147483648.0);
		case AV_SAMPLE_FMT_FLT: case AV_SAMPLE_FMT_FLTP:
			return static_cast<const float*>(static_cast<const void*>(data))[offset];
		case AV_SAMPLE_FMT_DBL: case AV_SAMPLE_FMT_DBLP:
			return static_cast<float>(static_cast<const double*>(static_cast<const void*>(data))[offset]);
		default:
			return 0.0f;
		}
	}

	void CaptureLoop()
	{
		while (!close_requested) {
			const int read_result = av_read_frame(format_context, packet);
			if (read_result == AVERROR(EAGAIN)) continue;
			if (read_result < 0) break;
			if (packet->stream_index != audio_stream) {
				av_packet_unref(packet);
				continue;
			}
			const int send_result = avcodec_send_packet(codec_context, packet);
			av_packet_unref(packet);
			if (send_result < 0) continue;
			while (avcodec_receive_frame(codec_context, decoded_frame) == 0) {
				const int64_t timestamp = decoded_frame->best_effort_timestamp != AV_NOPTS_VALUE
					? decoded_frame->best_effort_timestamp : decoded_frame->pts;
				if (timestamp == AV_NOPTS_VALUE) {
					// A packet with unknown capture time cannot be aligned safely.
					av_frame_unref(decoded_frame);
					continue;
				}
				const int64_t timestamp_us = av_rescale_q(timestamp,
					format_context->streams[audio_stream]->time_base, AVRational{1, AV_TIME_BASE});
				std::vector<std::vector<float>> samples(settings.audio_channels,
					std::vector<float>(decoded_frame->nb_samples));
				for (int channel = 0; channel < settings.audio_channels; ++channel) {
					for (int sample = 0; sample < decoded_frame->nb_samples; ++sample) {
						samples[channel][sample] = SampleAt(decoded_frame, channel, sample);
					}
				}
				std::lock_guard<std::mutex> lock(queue_mutex);
				audio_buffer.PushTimestamp(timestamp_us, epoch_us, AVRational{1, AV_TIME_BASE},
					settings.audio_sample_rate, std::move(samples));
				av_frame_unref(decoded_frame);
				ready.notify_all();
			}
		}
	}

	ScreenCaptureSettings settings;
	AVFormatContext* format_context = nullptr;
	AVCodecContext* codec_context = nullptr;
	AVPacket* packet = nullptr;
	AVFrame* decoded_frame = nullptr;
	int audio_stream = -1;
	std::atomic<bool> close_requested { false };
	std::thread worker;
	std::mutex queue_mutex;
	std::condition_variable ready;
	CaptureAudioBuffer audio_buffer;
	int64_t epoch_us = 0;
	int64_t last_output_frame = 0;
	bool timeline_started = false;
};
#elif defined(_WIN32)
class ScreenCaptureReader::SystemAudioCapture
{
public:
	explicit SystemAudioCapture(const ScreenCaptureSettings& new_settings)
		: settings(new_settings) {}

	~SystemAudioCapture() { Close(); }

	void Open()
	{
		if (worker.joinable()) return;
		close_requested = false;
		worker = std::thread(&SystemAudioCapture::CaptureLoop, this);
		std::unique_lock<std::mutex> lock(state_mutex);
		state_ready.wait_for(lock, std::chrono::seconds(5), [this]() { return opened || failed; });
		if (!opened) {
			close_requested = true;
			lock.unlock();
			if (worker.joinable()) worker.join();
			throw InvalidOptions(error.empty() ? "Unable to open WASAPI system audio loopback capture." : error);
		}
	}

	void Close()
	{
		close_requested = true;
		ready.notify_all();
		if (worker.joinable()) worker.join();
		opened = false;
	}

	void AddFrameAudio(const std::shared_ptr<Frame>& frame, int64_t number, const Fraction& fps)
	{
		int sample_count = 0;
		for (int64_t frame_number = last_output_frame + 1; frame_number <= number; ++frame_number) {
			sample_count += Frame::GetSamplesPerFrame(frame_number, fps, sample_rate, channel_count);
		}
		last_output_frame = std::max(last_output_frame, number);
		if (!frame || sample_count <= 0) return;
		std::unique_lock<std::mutex> lock(queue_mutex);
		const auto wait_time = timeline_started
			? std::chrono::milliseconds(100)
			: std::chrono::milliseconds(3000);
		ready.wait_for(lock, wait_time, [this, sample_count]() {
			return close_requested || audio_buffer.Covers(sample_count);
		});
		timeline_started = true;
		auto samples = audio_buffer.Read(sample_count);
		frame->SampleRate(sample_rate);
		frame->ChannelsLayout(channel_count == 1 ? LAYOUT_MONO : LAYOUT_STEREO);
		for (int channel = 0; channel < channel_count; ++channel) {
			frame->AddAudio(true, channel, 0, samples[channel].data(), sample_count, 1.0f);
		}
	}

	void Reset()
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		// WASAPI returns QPC timestamps already converted to 100 ns units.
		LARGE_INTEGER counter, frequency;
		QueryPerformanceCounter(&counter);
		QueryPerformanceFrequency(&frequency);
		epoch_100ns = av_rescale(counter.QuadPart, 10000000, frequency.QuadPart);
		audio_buffer.Reset(channel_count, static_cast<int64_t>(sample_rate) * 10);
		last_output_frame = 0;
		timeline_started = false;
	}

	int SampleRate() const { return sample_rate; }
	int Channels() const { return channel_count; }

private:
	template <typename T> static void Release(T*& value)
	{
		if (value) { value->Release(); value = nullptr; }
	}

	void Fail(const std::string& message)
	{
		std::lock_guard<std::mutex> lock(state_mutex);
		error = message;
		failed = true;
		state_ready.notify_all();
	}

	void CaptureLoop()
	{
		IMMDeviceEnumerator* enumerator = nullptr;
		IMMDevice* device = nullptr;
		IAudioClient* client = nullptr;
		IAudioCaptureClient* capture = nullptr;
		WAVEFORMATEX* format = nullptr;
		const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		const bool uninitialize_com = SUCCEEDED(com_result);
		HRESULT result = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
			IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&enumerator));
		if (SUCCEEDED(result)) {
			result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
		}
		if (SUCCEEDED(result)) {
			result = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
		}
		if (SUCCEEDED(result)) result = client->GetMixFormat(&format);
		if (SUCCEEDED(result)) {
			result = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
				0, 0, format, nullptr);
		}
		if (SUCCEEDED(result)) {
			result = client->GetService(IID_IAudioCaptureClient, reinterpret_cast<void**>(&capture));
		}
		if (SUCCEEDED(result)) result = client->Start();
		if (FAILED(result) || !format || !capture) {
			Fail("Unable to initialize WASAPI system audio loopback capture (HRESULT " + std::to_string(result) + ").");
			if (format) CoTaskMemFree(format);
			Release(capture); Release(client); Release(device); Release(enumerator);
			if (uninitialize_com) CoUninitialize();
			return;
		}

		sample_rate = static_cast<int>(format->nSamplesPerSec);
		channel_count = std::max(1, std::min(2, static_cast<int>(format->nChannels)));
		Reset();
		bool floating_point = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
		if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
			const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
			floating_point = extensible->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
		}
		{
			std::lock_guard<std::mutex> lock(state_mutex);
			opened = true;
			state_ready.notify_all();
		}

		while (!close_requested) {
			UINT32 packet_frames = 0;
			if (FAILED(capture->GetNextPacketSize(&packet_frames))) break;
			if (packet_frames == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
				continue;
			}
			BYTE* data = nullptr;
			UINT32 frames = 0;
			DWORD flags = 0;
			UINT64 timestamp_100ns = 0;
			if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, &timestamp_100ns))) break;
			{
				std::vector<std::vector<float>> samples(channel_count, std::vector<float>(frames));
				for (UINT32 frame_index = 0; frame_index < frames; ++frame_index) {
					for (int channel = 0; channel < channel_count; ++channel) {
						float sample = 0.0f;
						if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data) {
							const int offset = static_cast<int>(frame_index) * format->nChannels + channel;
							if (floating_point && format->wBitsPerSample == 32) {
								sample = reinterpret_cast<const float*>(data)[offset];
							} else if (format->wBitsPerSample == 16) {
								sample = reinterpret_cast<const int16_t*>(data)[offset] / 32768.0f;
							} else if (format->wBitsPerSample == 32) {
								sample = static_cast<float>(reinterpret_cast<const int32_t*>(data)[offset] / 2147483648.0);
							}
						}
						samples[channel][frame_index] = sample;
					}
				}
				// Preserve timestamp gaps (including silence/discontinuities). Never
				// invent a new epoch from packet arrival time after a delayed read.
				if (!(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)) {
					std::lock_guard<std::mutex> lock(queue_mutex);
					audio_buffer.PushTimestamp(static_cast<int64_t>(timestamp_100ns), epoch_100ns,
						AVRational{1, 10000000}, sample_rate, std::move(samples));
				}
			}
			capture->ReleaseBuffer(frames);
			ready.notify_all();
		}

		client->Stop();
		CoTaskMemFree(format);
		Release(capture); Release(client); Release(device); Release(enumerator);
		if (uninitialize_com) CoUninitialize();
	}

	ScreenCaptureSettings settings;
	std::atomic<bool> close_requested { false };
	std::thread worker;
	std::mutex queue_mutex;
	std::condition_variable ready;
	CaptureAudioBuffer audio_buffer;
	int64_t epoch_100ns = 0;
	int64_t last_output_frame = 0;
	bool timeline_started = false;
	int sample_rate = 48000;
	int channel_count = 2;
	std::mutex state_mutex;
	std::condition_variable state_ready;
	bool opened = false;
	bool failed = false;
	std::string error;
};
#else
class ScreenCaptureReader::SystemAudioCapture
{
public:
	explicit SystemAudioCapture(const ScreenCaptureSettings&) {}
	void Open() {}
	void Close() {}
	void AddFrameAudio(const std::shared_ptr<Frame>&, int64_t, const Fraction&) {}
	void Reset() {}
	int SampleRate() const { return 0; }
	int Channels() const { return 0; }
};
#endif

namespace
{
	std::string fraction_to_string(const Fraction& value)
	{
		std::stringstream stream;
		stream << value.num << "/" << value.den;
		return stream.str();
	}

	void set_option(AVDictionary** options, const std::string& key, const std::string& value)
	{
		av_dict_set(options, key.c_str(), value.c_str(), 0);
	}

	bool option_enabled(const std::map<std::string, std::string>& options, const std::string& key)
	{
		const auto option = options.find(key);
		if (option == options.end()) {
			return false;
		}
		return option->second == "1" || option->second == "true" || option->second == "yes";
	}

	int option_int(const std::map<std::string, std::string>& options, const std::string& key, int fallback)
	{
		const auto option = options.find(key);
		if (option == options.end()) {
			return fallback;
		}
		try {
			return std::stoi(option->second);
		} catch (...) {
			return fallback;
		}
	}

	int capture_interrupt_callback(void* opaque)
	{
		const auto* reader = static_cast<std::atomic<bool>*>(opaque);
		return reader && reader->load() ? 1 : 0;
	}

	std::string avfoundation_video_name(const std::string& input_name)
	{
		const size_t separator = input_name.find(':');
		return separator == std::string::npos ? input_name : input_name.substr(0, separator);
	}

	std::string avfoundation_audio_name(const std::string& input_name)
	{
		const size_t separator = input_name.find(':');
		return separator == std::string::npos ? "none" : input_name.substr(separator + 1);
	}

	bool resolve_avfoundation_video_index(
		AVInputFormat* input_format,
		const std::string& video_name,
		std::string& video_index)
	{
		const auto is_digit = [](unsigned char value) {
			return std::isdigit(value) != 0;
		};
		if (video_name.empty()
			|| video_name == "default"
			|| video_name == "none"
			|| std::all_of(video_name.begin(), video_name.end(), is_digit)) {
			return false;
		}

		AVDeviceInfoList* device_list = nullptr;
		const int result = avdevice_list_input_sources(input_format, nullptr, nullptr, &device_list);
		if (result < 0 || !device_list) {
			avdevice_free_list_devices(&device_list);
			return false;
		}

		bool found = false;
		for (int index = 0; index < device_list->nb_devices; ++index) {
			const AVDeviceInfo* device = device_list->devices[index];
			if (!device || !device->device_name) {
				continue;
			}
			const std::string name = device->device_name;
			const std::string description = device->device_description
				? device->device_description
				: device->device_name;
			if (video_name == name || video_name == description) {
				video_index = std::all_of(name.begin(), name.end(), is_digit)
					? name
					: std::to_string(index);
				found = true;
				break;
			}
		}
		avdevice_free_list_devices(&device_list);
		return found;
	}
}

ScreenCaptureReader::ScreenCaptureReader(const ScreenCaptureSettings& new_settings)
	: settings(new_settings)
	, backend_reader(nullptr)
	, backend_module(nullptr)
	, is_open(false)
	, video_stream(-1)
	, frames_read(0)
	, dropped_packets(0)
	, format_context(nullptr)
	, codec_context(nullptr)
	, source_frame(nullptr)
	, rgba_frame(nullptr)
	, packet(nullptr)
	, sws_context(nullptr)
	, close_requested(false)
	, system_audio(nullptr)
{
	if (settings.backend == SCREEN_CAPTURE_AUTO) {
		settings.backend = DefaultBackend();
	}
	ValidateSettings();
	PopulateInfo();
#if defined(__linux__) || defined(_WIN32)
	if (settings.capture_audio) {
		system_audio = std::make_unique<SystemAudioCapture>(settings);
	}
#endif
	if (UsesWaylandPortal()) {
	#if defined(HAVE_WAYLAND_CAPTURE_PLUGIN) && defined(__linux__)
		std::string module_error;
		backend_module = load_wayland_capture_backend(module_error);
		if (!backend_module) {
			throw InvalidOptions("Wayland screen capture backend is unavailable: "
				+ module_error);
		}
		auto factory = reinterpret_cast<WaylandBackendFactory>(
			dlsym(backend_module, "OpenShotCreateWaylandScreenCaptureReader"));
		if (!factory) {
			const char* error = dlerror();
			dlclose(backend_module);
			backend_module = nullptr;
			throw InvalidOptions("Wayland screen capture backend is invalid: "
				+ std::string(error ? error : "factory function is missing."));
		}
		try {
			backend_reader = factory(settings, info);
		} catch (...) {
			dlclose(backend_module);
			backend_module = nullptr;
			throw;
		}
		if (!backend_reader) {
			dlclose(backend_module);
			backend_module = nullptr;
			throw InvalidOptions("Wayland screen capture backend is unavailable.");
		}
	#else
		throw InvalidOptions("Wayland screen capture backend is unavailable in this build.");
	#endif
	}
}

ScreenCaptureReader::~ScreenCaptureReader()
{
	Close();
#if defined(__linux__)
	backend_reader.reset();
	if (backend_module) {
		dlclose(backend_module);
		backend_module = nullptr;
	}
#endif
}

bool ScreenCaptureReader::IsOpen()
{
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);
	return backend_reader ? backend_reader->IsOpen() : is_open;
}

bool ScreenCaptureReader::IsBackendSupported(ScreenCaptureBackend backend)
{
#if defined(__linux__)
	if (backend == SCREEN_CAPTURE_X11 || backend == SCREEN_CAPTURE_AUTO) {
		return true;
	}
#if defined(HAVE_WAYLAND_CAPTURE_PLUGIN)
	if (backend == SCREEN_CAPTURE_WAYLAND) {
		return true;
	}
#endif
	return false;
#elif defined(_WIN32)
	return backend == SCREEN_CAPTURE_WINDOWS_GDI || backend == SCREEN_CAPTURE_AUTO;
#elif defined(__APPLE__)
	return backend == SCREEN_CAPTURE_MAC_AVFOUNDATION || backend == SCREEN_CAPTURE_AUTO;
#else
	(void) backend;
	return false;
#endif
}

bool ScreenCaptureReader::IsSystemAudioSupported(ScreenCaptureBackend backend)
{
#if defined(__linux__)
	if (backend == SCREEN_CAPTURE_AUTO) {
		backend = DefaultBackend();
	}
	avdevice_register_all();
	return (backend == SCREEN_CAPTURE_X11 || backend == SCREEN_CAPTURE_WAYLAND)
		&& av_find_input_format("pulse") != nullptr;
#elif defined(_WIN32)
	return backend == SCREEN_CAPTURE_AUTO || backend == SCREEN_CAPTURE_WINDOWS_GDI;
#else
	(void) backend;
	return false;
#endif
}

ScreenCaptureBackend ScreenCaptureReader::DefaultBackend()
{
#if defined(__linux__)
	const char* session = std::getenv("XDG_SESSION_TYPE");
	if (session && std::string(session) == "wayland" && IsBackendSupported(SCREEN_CAPTURE_WAYLAND)) {
		return SCREEN_CAPTURE_WAYLAND;
	}
	if (!session || std::string(session) == "x11") {
		return SCREEN_CAPTURE_X11;
	}
#elif defined(_WIN32)
	return SCREEN_CAPTURE_WINDOWS_GDI;
#elif defined(__APPLE__)
	return SCREEN_CAPTURE_MAC_AVFOUNDATION;
#endif
	return SCREEN_CAPTURE_AUTO;
}

void ScreenCaptureReader::ValidateSettings() const
{
	if (!IsBackendSupported(settings.backend)) {
		throw InvalidOptions("Screen capture backend is not supported on this OS or session.");
	}
	if (!UsesFFmpegDevice() && !UsesWaylandPortal()) {
		throw InvalidOptions("Screen capture backend is not implemented in this build.");
	}
	if (settings.width <= 0 || settings.height <= 0) {
		throw InvalidOptions("Screen capture requires a positive width and height.");
	}
	if (settings.fps.num <= 0 || settings.fps.den <= 0) {
		throw InvalidOptions("Screen capture requires a positive frame rate.");
	}
	if (settings.capture_audio && !IsSystemAudioSupported(settings.backend)) {
		throw InvalidOptions("System audio capture is not supported by this screen capture backend.");
	}
	if (settings.audio_sample_rate < 8000) {
		throw InvalidSampleRate("System audio capture requires a sample rate of at least 8000 Hz.");
	}
	if (settings.audio_channels < 1 || settings.audio_channels > 2) {
		throw InvalidChannels("System audio capture requires one or two channels.");
	}
}

bool ScreenCaptureReader::UsesFFmpegDevice() const
{
	return settings.backend == SCREEN_CAPTURE_X11
		|| settings.backend == SCREEN_CAPTURE_WINDOWS_GDI
		|| settings.backend == SCREEN_CAPTURE_MAC_AVFOUNDATION
		|| settings.options.count("input_format_name");
}

bool ScreenCaptureReader::UsesWaylandPortal() const
{
	return settings.backend == SCREEN_CAPTURE_WAYLAND;
}

std::string ScreenCaptureReader::InputFormatName() const
{
	const auto override_format = settings.options.find("input_format_name");
	if (override_format != settings.options.end()) {
		return override_format->second;
	}
	if (settings.backend == SCREEN_CAPTURE_X11) {
		return "x11grab";
	}
	if (settings.backend == SCREEN_CAPTURE_WINDOWS_GDI) {
		return "gdigrab";
	}
	if (settings.backend == SCREEN_CAPTURE_MAC_AVFOUNDATION) {
		return "avfoundation";
	}
	return "";
}

std::string ScreenCaptureReader::InputName() const
{
	if (InputFormatName() == "v4l2") {
		return settings.display.empty() ? "/dev/video0" : settings.display;
	}
	if (InputFormatName() == "dshow") {
		return settings.display;
	}
	if (InputFormatName() == "gdigrab") {
		return settings.display.empty() ? "desktop" : settings.display;
	}
	if (InputFormatName() == "avfoundation") {
		return settings.display.empty() ? "Capture screen 0:none" : settings.display;
	}

	std::string display = settings.display;
	if (display.empty()) {
		const char* env_display = std::getenv("DISPLAY");
		display = env_display ? env_display : ":0.0";
	}
	if (InputFormatName() == "x11grab" && settings.options.count("window_id")) {
		return display;
	}

	std::stringstream input;
	input << display << "+" << settings.x << "," << settings.y;
	return input.str();
}

void ScreenCaptureReader::PopulateInfo()
{
	info.has_video = true;
	info.has_audio = settings.capture_audio;
	info.has_single_image = false;
	info.duration = 60.0f * 60.0f;
	info.file_size = 0;
	info.width = settings.width;
	info.height = settings.height;
	info.pixel_format = AV_PIX_FMT_RGBA;
	info.fps = settings.fps;
	info.video_bit_rate = 0;
	info.pixel_ratio = Fraction(1, 1);
	info.display_ratio = Fraction(settings.width, settings.height);
	info.display_ratio.Reduce();
	info.vcodec = "rawvideo";
	info.video_length = static_cast<int64_t>(info.duration * settings.fps.ToFloat());
	info.video_stream_index = -1;
	info.video_timebase = settings.fps.Reciprocal();
	info.interlaced_frame = false;
	info.top_field_first = false;
	info.acodec = settings.capture_audio ? "pcm_f32le" : "";
	info.audio_bit_rate = 0;
	info.sample_rate = settings.capture_audio ? settings.audio_sample_rate : 0;
	info.channels = settings.capture_audio ? settings.audio_channels : 0;
	info.channel_layout = settings.audio_channels == 2 ? LAYOUT_STEREO : LAYOUT_MONO;
	info.audio_stream_index = -1;
	info.audio_timebase = Fraction(1, 1);
}

void ScreenCaptureReader::Open()
{
	if (backend_reader) {
		if (backend_reader->IsOpen()) return;
		manual_system_audio = false;
		if (system_audio) {
			system_audio->Open();
			info.sample_rate = system_audio->SampleRate();
			info.channels = system_audio->Channels();
			info.channel_layout = info.channels == 1 ? LAYOUT_MONO : LAYOUT_STEREO;
		}
		try {
			backend_reader->Open();
		} catch (...) {
			if (system_audio) system_audio->Close();
			throw;
		}
		return;
	}
	// Close() may run on the UI thread while a worker is opening the device.
	// Serialize initialization with frame reads and cleanup to keep FFmpeg's
	// context alive until avformat_open_input/OpenDecoder have finished.
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);
	if (is_open) {
		return;
	}
	manual_system_audio = false;
	close_requested = false;
	try {
		if (system_audio) {
			system_audio->Open();
			info.sample_rate = system_audio->SampleRate();
			info.channels = system_audio->Channels();
			info.channel_layout = info.channels == 1 ? LAYOUT_MONO : LAYOUT_STEREO;
		}
		OpenDevice();
		OpenDecoder();
		if (close_requested) {
			throw ReaderClosed("Capture initialization was cancelled.");
		}
	} catch (...) {
		// Also release partially initialized device/decoder resources. The
		// recursive lifecycle lock permits using the normal cleanup path here.
		Close();
		throw;
	}
	is_open = true;
}

void ScreenCaptureReader::OpenDevice()
{
	avdevice_register_all();

	AVInputFormat* input_format = const_cast<AVInputFormat*>(av_find_input_format(InputFormatName().c_str()));
	std::string input_name = InputName();
	if (!input_format) {
		throw InvalidOptions("FFmpeg input device is not available: " + InputFormatName(), input_name);
	}

	format_context = avformat_alloc_context();
	if (!format_context) {
		throw OutOfMemory("Unable to allocate capture format context.", input_name);
	}
	format_context->interrupt_callback.callback = capture_interrupt_callback;
	format_context->interrupt_callback.opaque = &close_requested;

	AVDictionary* options = nullptr;
	const bool use_device_defaults = settings.options.count("use_device_defaults") > 0;
	const bool post_capture_crop = option_enabled(settings.options, "crop_after_capture");
	if (!use_device_defaults && !post_capture_crop) {
		set_option(&options, "framerate", fraction_to_string(settings.fps));
		set_option(&options, "video_size", std::to_string(settings.width) + "x" + std::to_string(settings.height));
	} else if (post_capture_crop) {
		const int source_width = option_int(settings.options, "crop_source_width", settings.width);
		const int source_height = option_int(settings.options, "crop_source_height", settings.height);
		set_option(&options, "framerate", fraction_to_string(settings.fps));
		set_option(&options, "video_size", std::to_string(source_width) + "x" + std::to_string(source_height));
	}
	if (InputFormatName() == "x11grab") {
		set_option(&options, "draw_mouse", settings.include_cursor ? "1" : "0");
		set_option(&options, "show_region", settings.show_region ? "1" : "0");
	} else if (InputFormatName() == "gdigrab") {
		set_option(&options, "draw_mouse", settings.include_cursor ? "1" : "0");
		set_option(&options, "show_region", settings.show_region ? "1" : "0");
		set_option(&options, "offset_x", std::to_string(settings.x));
		set_option(&options, "offset_y", std::to_string(settings.y));
	} else if (InputFormatName() == "avfoundation") {
		set_option(&options, "capture_cursor", settings.include_cursor ? "1" : "0");
		std::string video_index;
		if (resolve_avfoundation_video_index(input_format, avfoundation_video_name(input_name), video_index)) {
			set_option(&options, "video_device_index", video_index);
			input_name = ":" + avfoundation_audio_name(input_name);
		}
	}
	for (const auto& option : settings.options) {
		if (option.first == "input_format_name"
			|| option.first == "use_device_defaults"
			|| option.first == "crop_after_capture"
			|| option.first == "crop_source_width"
			|| option.first == "crop_source_height") {
			continue;
		}
		set_option(&options, option.first, option.second);
	}

	const int result = avformat_open_input(&format_context, input_name.c_str(), input_format, &options);
	av_dict_free(&options);
	if (result < 0) {
		throw InvalidFile("Unable to open screen capture input: " + std::string(av_err2str(result)), input_name);
	}
}

void ScreenCaptureReader::OpenDecoder()
{
	if (avformat_find_stream_info(format_context, nullptr) < 0) {
		throw InvalidFile("Unable to read capture stream information.", InputName());
	}

	for (unsigned int index = 0; index < format_context->nb_streams; ++index) {
		if (format_context->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream = static_cast<int>(index);
			break;
		}
	}
	if (video_stream < 0) {
		throw InvalidFile("No video stream found in capture input.", InputName());
	}

	AVStream* stream = format_context->streams[video_stream];
	const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
	if (!codec) {
		throw InvalidCodec("No decoder available for capture stream.", InputName());
	}

	codec_context = ffmpeg_get_codec_context(stream, codec);
	if (!codec_context || avcodec_open2(codec_context, codec, nullptr) < 0) {
		throw InvalidCodec("Unable to open capture stream decoder.", InputName());
	}

	info.width = codec_context->width > 0 ? codec_context->width : settings.width;
	info.height = codec_context->height > 0 ? codec_context->height : settings.height;
	if (option_enabled(settings.options, "crop_after_capture")) {
		info.width = settings.width;
		info.height = settings.height;
	}
	info.video_stream_index = video_stream;
	info.pixel_format = codec_context->pix_fmt;
	info.display_ratio = Fraction(info.width, info.height);
	info.display_ratio.Reduce();
	if (codec_context->framerate.num > 0 && codec_context->framerate.den > 0) {
		info.fps = Fraction(codec_context->framerate.num, codec_context->framerate.den);
		info.video_timebase = info.fps.Reciprocal();
	} else if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
		info.fps = Fraction(stream->avg_frame_rate.num, stream->avg_frame_rate.den);
		info.video_timebase = info.fps.Reciprocal();
	} else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
		info.fps = Fraction(stream->r_frame_rate.num, stream->r_frame_rate.den);
		info.video_timebase = info.fps.Reciprocal();
	}

	source_frame = av_frame_alloc();
	rgba_frame = av_frame_alloc();
	packet = av_packet_alloc();
	if (!source_frame || !rgba_frame || !packet) {
		throw OutOfMemory("Unable to allocate capture decoder frames.", InputName());
	}
}

std::shared_ptr<Frame> ScreenCaptureReader::GetFrame(int64_t number)
{
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);
	if (backend_reader) {
		if (!backend_reader->IsOpen()) {
			throw ReaderClosed("The ScreenCaptureReader is closed. Call Open() before GetFrame().");
		}
		auto frame = backend_reader->GetFrame(number);
		if (system_audio && !manual_system_audio) system_audio->AddFrameAudio(frame, number, info.fps);
		return frame;
	}
	if (!is_open) {
		throw ReaderClosed("The ScreenCaptureReader is closed. Call Open() before GetFrame().");
	}

	auto frame = DecodeNextFrame(number);
	if (system_audio && !manual_system_audio) system_audio->AddFrameAudio(frame, number, info.fps);
	return frame;
}

void ScreenCaptureReader::AddSystemAudio(std::shared_ptr<Frame> frame, int64_t output_frame_number)
{
	if (system_audio) {
		system_audio->AddFrameAudio(frame, output_frame_number, info.fps);
	}
}

void ScreenCaptureReader::ResetSystemAudio()
{
	manual_system_audio = true;
	if (system_audio) {
		system_audio->Reset();
	}
}

std::shared_ptr<Frame> ScreenCaptureReader::DecodeNextFrame(int64_t number)
{
	while (!close_requested) {
		const int read_result = av_read_frame(format_context, packet);
		if (read_result == AVERROR(EAGAIN)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}
		if (read_result < 0) {
			const std::string message = frames_read > 0
				? "Capture input ended after " + std::to_string(frames_read) + " frame(s): "
				: "Capture input ended before a frame could be read: ";
			throw InvalidFile(message + std::string(av_err2str(read_result)), InputName());
		}

		if (packet->stream_index != video_stream) {
			av_packet_unref(packet);
			dropped_packets++;
			continue;
		}

		const int64_t packet_timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
		const int send_result = avcodec_send_packet(codec_context, packet);
		av_packet_unref(packet);
		if (send_result < 0) {
			continue;
		}

		const int receive_result = avcodec_receive_frame(codec_context, source_frame);
		if (receive_result == AVERROR(EAGAIN)) {
			continue;
		}
		if (receive_result < 0) {
			throw InvalidFile("Unable to decode capture frame: " + std::string(av_err2str(receive_result)), InputName());
		}

		const AVStream* stream = format_context->streams[video_stream];
		int64_t frame_timestamp = source_frame->best_effort_timestamp;
		if (frame_timestamp == AV_NOPTS_VALUE) {
			frame_timestamp = source_frame->pts;
		}
		if (frame_timestamp == AV_NOPTS_VALUE) {
			frame_timestamp = packet_timestamp;
		}
		double capture_timestamp = std::numeric_limits<double>::quiet_NaN();
		if (frame_timestamp != AV_NOPTS_VALUE && stream) {
			capture_timestamp = static_cast<double>(frame_timestamp) * av_q2d(stream->time_base);
		}

		const int width = source_frame->width > 0 ? source_frame->width : info.width;
		const int height = source_frame->height > 0 ? source_frame->height : info.height;
		const PixelFormat src_fmt = static_cast<PixelFormat>(source_frame->format);

		sws_context = sws_getCachedContext(
			sws_context,
			width,
			height,
			src_fmt,
			width,
			height,
			AV_PIX_FMT_RGBA,
			SWS_BILINEAR,
			nullptr,
			nullptr,
			nullptr);
		if (!sws_context) {
			throw InvalidFile("Unable to create capture pixel conversion context.", InputName());
		}

		const int bytes_per_pixel = 4;
		const size_t buffer_size = static_cast<size_t>(width) * height * bytes_per_pixel;
		unsigned char* buffer = static_cast<unsigned char*>(aligned_malloc(buffer_size));
		if (!buffer) {
			throw OutOfMemory("Unable to allocate capture frame buffer.", InputName());
		}

		av_image_fill_arrays(rgba_frame->data, rgba_frame->linesize, buffer, AV_PIX_FMT_RGBA, width, height, 1);
		const int scaled_lines = sws_scale(sws_context, source_frame->data, source_frame->linesize, 0, height, rgba_frame->data, rgba_frame->linesize);
		av_frame_unref(source_frame);

		if (scaled_lines <= 0) {
			openshot::aligned_free(buffer);
			continue;
		}

		int output_width = width;
		int output_height = height;
		unsigned char* output_buffer = buffer;
		if (option_enabled(settings.options, "crop_after_capture")) {
			const int crop_x = std::max(0, std::min(settings.x, width - 1));
			const int crop_y = std::max(0, std::min(settings.y, height - 1));
			output_width = std::max(1, std::min(settings.width, width - crop_x));
			output_height = std::max(1, std::min(settings.height, height - crop_y));
			const size_t output_buffer_size = static_cast<size_t>(output_width) * output_height * bytes_per_pixel;
			output_buffer = static_cast<unsigned char*>(aligned_malloc(output_buffer_size));
			if (!output_buffer) {
				openshot::aligned_free(buffer);
				throw OutOfMemory("Unable to allocate cropped capture frame buffer.", InputName());
			}
			for (int row = 0; row < output_height; ++row) {
				const unsigned char* source_row = buffer
					+ (static_cast<size_t>(crop_y + row) * width + crop_x) * bytes_per_pixel;
				unsigned char* dest_row = output_buffer + static_cast<size_t>(row) * output_width * bytes_per_pixel;
				std::copy(source_row, source_row + static_cast<size_t>(output_width) * bytes_per_pixel, dest_row);
			}
			openshot::aligned_free(buffer);
		}

		auto frame = std::make_shared<Frame>(number, output_width, output_height, "#000000");
		frame->capture_timestamp = capture_timestamp;
		frame->AddImage(output_width, output_height, bytes_per_pixel, QImage::Format_RGBA8888, output_buffer);
		frames_read++;
		return frame;
	}

	throw ReaderClosed("The ScreenCaptureReader was closed.");
}

void ScreenCaptureReader::Close()
{
	// Signal blocking native reads before waiting for GetFrame(). The FFmpeg
	// interrupt callback observes close_requested, while backend readers use
	// Close() to wake their own blocking waits.
	close_requested = true;
	if (backend_reader) {
		backend_reader->Close();
	}

	// Open() and GetFrame() own FFmpeg and system-audio use under this mutex.
	// Wait for initialization or an interrupted read before releasing resources.
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);
	if (system_audio) {
		system_audio->Close();
	}
	if (packet) {
		av_packet_free(&packet);
	}
	if (source_frame) {
		av_frame_free(&source_frame);
	}
	if (rgba_frame) {
		av_frame_free(&rgba_frame);
	}
	if (sws_context) {
		sws_freeContext(sws_context);
		sws_context = nullptr;
	}
	if (codec_context) {
		avcodec_free_context(&codec_context);
	}
	if (format_context) {
		avformat_close_input(&format_context);
	}
	is_open = false;
	video_stream = -1;
}

openshot::CaptureReaderStats ScreenCaptureReader::GetStats() const
{
	if (backend_reader) {
		return backend_reader->GetStats();
	}
	CaptureReaderStats stats;
	stats.is_open = is_open;
	stats.frames_read = frames_read;
	stats.dropped_packets = dropped_packets;
	const double fps = settings.fps.den != 0 ? static_cast<double>(settings.fps.num) / static_cast<double>(settings.fps.den) : 0.0;
	stats.duration = fps > 0.0 ? static_cast<double>(frames_read) / fps : 0.0;
	return stats;
}

std::string ScreenCaptureReader::Json() const
{
	return JsonValue().toStyledString();
}

Json::Value ScreenCaptureReader::JsonValue() const
{
	Json::Value root = ReaderBase::JsonValue();
	root["type"] = "ScreenCaptureReader";
	root["backend"] = settings.backend;
	root["display"] = settings.display;
	root["x"] = settings.x;
	root["y"] = settings.y;
	root["width"] = settings.width;
	root["height"] = settings.height;
	root["fps"]["num"] = settings.fps.num;
	root["fps"]["den"] = settings.fps.den;
	root["include_cursor"] = settings.include_cursor;
	root["show_region"] = settings.show_region;
	root["capture_audio"] = settings.capture_audio;
	root["audio_device"] = settings.audio_device;
	root["audio_sample_rate"] = settings.audio_sample_rate;
	root["audio_channels"] = settings.audio_channels;
	root["options"] = Json::Value(Json::objectValue);
	for (const auto& option : settings.options) {
		root["options"][option.first] = option.second;
	}
	return root;
}

void ScreenCaptureReader::SetJson(const std::string value)
{
	try {
		SetJsonValue(openshot::stringToJson(value));
	} catch (const std::exception&) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void ScreenCaptureReader::SetJsonValue(const Json::Value root)
{
	if (!root["backend"].isNull())
		settings.backend = static_cast<ScreenCaptureBackend>(root["backend"].asInt());
	if (!root["display"].isNull())
		settings.display = root["display"].asString();
	if (!root["x"].isNull())
		settings.x = root["x"].asInt();
	if (!root["y"].isNull())
		settings.y = root["y"].asInt();
	if (!root["width"].isNull())
		settings.width = root["width"].asInt();
	if (!root["height"].isNull())
		settings.height = root["height"].asInt();
	if (!root["fps"].isNull() && root["fps"].isObject()) {
		if (!root["fps"]["num"].isNull())
			settings.fps.num = root["fps"]["num"].asInt();
		if (!root["fps"]["den"].isNull())
			settings.fps.den = root["fps"]["den"].asInt();
	}
	if (!root["include_cursor"].isNull())
		settings.include_cursor = root["include_cursor"].asBool();
	if (!root["show_region"].isNull())
		settings.show_region = root["show_region"].asBool();
	if (!root["capture_audio"].isNull())
		settings.capture_audio = root["capture_audio"].asBool();
	if (!root["audio_device"].isNull())
		settings.audio_device = root["audio_device"].asString();
	if (!root["audio_sample_rate"].isNull())
		settings.audio_sample_rate = root["audio_sample_rate"].asInt();
	if (!root["audio_channels"].isNull())
		settings.audio_channels = root["audio_channels"].asInt();
	if (!root["options"].isNull() && root["options"].isObject()) {
		settings.options.clear();
		for (const auto& key : root["options"].getMemberNames()) {
			settings.options[key] = root["options"][key].asString();
		}
	}
	if (settings.backend == SCREEN_CAPTURE_AUTO) {
		settings.backend = DefaultBackend();
	}
	ValidateSettings();
	PopulateInfo();
#if defined(__linux__) || defined(_WIN32)
	system_audio.reset();
	if (settings.capture_audio) {
		system_audio = std::make_unique<SystemAudioCapture>(settings);
	}
#endif
}
