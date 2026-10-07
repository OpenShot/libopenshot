/**
 * @file
 * @brief Header file for live screen capture readers
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_SCREENCAPTUREREADER_H
#define OPENSHOT_SCREENCAPTUREREADER_H

#include "ReaderBase.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>

#include "FFmpegUtilities.h"

namespace openshot
{
	enum ScreenCaptureBackend
	{
		SCREEN_CAPTURE_AUTO = 0,
		SCREEN_CAPTURE_X11 = 1,
		SCREEN_CAPTURE_WAYLAND = 2,
		SCREEN_CAPTURE_WINDOWS_GDI = 3,
		SCREEN_CAPTURE_MAC_AVFOUNDATION = 4
	};

	struct ScreenCaptureSettings
	{
		ScreenCaptureBackend backend = SCREEN_CAPTURE_AUTO;
		std::string display;
		int x = 0;
		int y = 0;
		int width = 1280;
		int height = 720;
		openshot::Fraction fps = openshot::Fraction(30, 1);
		bool include_cursor = true;
		bool show_region = false;
		bool capture_audio = false;
		std::string audio_device;
		int audio_sample_rate = 48000;
		int audio_channels = 2;
		std::map<std::string, std::string> options;
	};

	struct CaptureReaderStats
	{
		bool is_open = false;
		int64_t frames_read = 0;
		int dropped_packets = 0;
		double duration = 0.0;
	};

	class ScreenCaptureReader : public ReaderBase
	{
	public:
#ifndef SWIG
		class CaptureBackendReader
		{
		public:
			virtual ~CaptureBackendReader() = default;
			virtual void Open() = 0;
			virtual void Close() = 0;
			virtual bool IsOpen() const = 0;
			virtual std::shared_ptr<openshot::Frame> GetFrame(int64_t number) = 0;
			virtual openshot::CaptureReaderStats GetStats() const = 0;
		};
#endif

		explicit ScreenCaptureReader(const ScreenCaptureSettings& settings);
		~ScreenCaptureReader() override;

		void Close() override;
		CacheBase* GetCache() override { return NULL; };
		std::shared_ptr<openshot::Frame> GetFrame(int64_t number) override;
		bool IsOpen() override;
		std::string Name() override { return "ScreenCaptureReader"; };
		std::string Json() const override;
		void SetJson(const std::string value) override;
		Json::Value JsonValue() const override;
		void SetJsonValue(const Json::Value root) override;
		void Open() override;

		openshot::CaptureReaderStats GetStats() const;
		void AddSystemAudio(std::shared_ptr<openshot::Frame> frame, int64_t output_frame_number);
		void ResetSystemAudio();
		ScreenCaptureSettings GetSettings() const { return settings; };
		static bool IsBackendSupported(ScreenCaptureBackend backend);
		static bool IsSystemAudioSupported(ScreenCaptureBackend backend);
		static ScreenCaptureBackend DefaultBackend();

	private:
		void ValidateSettings() const;
		void OpenDevice();
		void OpenDecoder();
		std::string InputFormatName() const;
		std::string InputName() const;
		std::shared_ptr<openshot::Frame> DecodeNextFrame(int64_t number);
		bool UsesFFmpegDevice() const;
		bool UsesWaylandPortal() const;
		void PopulateInfo();

		ScreenCaptureSettings settings;
#ifndef SWIG
		std::unique_ptr<CaptureBackendReader> backend_reader;
		// The Wayland backend is a runtime-loaded module. Keep it loaded until its
		// reader is destroyed, since the reader's vtable lives in that module.
		void* backend_module = nullptr;
#endif
		bool is_open;
		int video_stream;
		int64_t frames_read;
		int dropped_packets;
		AVFormatContext* format_context;
		AVCodecContext* codec_context;
		AVFrame* source_frame;
		AVFrame* rgba_frame;
		AVPacket* packet;
		SwsContext* sws_context;
		std::atomic<bool> close_requested;
		bool manual_system_audio = false;
#ifndef SWIG
		class SystemAudioCapture;
		std::unique_ptr<SystemAudioCapture> system_audio;
#endif
	};
}

#endif
