/**
 * @file
 * @brief Experimental Vulkan benchmark comparing a CPU Qt path against an FFmpeg Vulkan path
 * @author OpenShot Studios, LLC
 *
 * @ref License
 */

// Copyright (c) 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QTransform>

#include "FFmpegReader.h"
#include "Frame.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <vulkan/vulkan.h>

using openshot::FFmpegReader;
using openshot::Fraction;
using openshot::Frame;

namespace {

using Clock = std::chrono::steady_clock;
using Micros = std::chrono::microseconds;

struct ScopedFrame {
	AVFrame* frame = nullptr;

	ScopedFrame() : frame(av_frame_alloc()) {}
	explicit ScopedFrame(AVFrame* value) : frame(value) {}

	~ScopedFrame() {
		if (frame)
			av_frame_free(&frame);
	}

	ScopedFrame(const ScopedFrame&) = delete;
	ScopedFrame& operator=(const ScopedFrame&) = delete;

	ScopedFrame(ScopedFrame&& other) noexcept : frame(other.frame) {
		other.frame = nullptr;
	}

	ScopedFrame& operator=(ScopedFrame&& other) noexcept {
		if (this != &other) {
			if (frame)
				av_frame_free(&frame);
			frame = other.frame;
			other.frame = nullptr;
		}
		return *this;
	}

	AVFrame* get() const { return frame; }
	AVFrame* release() {
		AVFrame* value = frame;
		frame = nullptr;
		return value;
	}
	operator bool() const { return frame != nullptr; }
};

struct ScopedPacket {
	AVPacket* packet = nullptr;

	ScopedPacket() : packet(av_packet_alloc()) {}
	~ScopedPacket() {
		if (packet)
			av_packet_free(&packet);
	}

	AVPacket* get() const { return packet; }
};

struct BenchmarkOptions {
	std::string video_path = std::string(TEST_MEDIA_PATH) + "keyframes.mp4";
	std::string overlay_path = std::string(TEST_MEDIA_PATH) + "front3.png";
	std::string vaapi_device = "/dev/dri/renderD128";
	std::string row_name;
	std::string mode = "preview";
	std::string dump_dir;
	int max_frames = 120;
	int output_width = 0;
	int output_height = 0;
	int preview_width = 854;
	int preview_height = 480;
	int dump_every = 0;
	double overlay_alpha = 0.72;
	double overlay_rotation = -8.0;
	bool run_cpu = true;
	bool run_vulkan = true;
};

struct RenderLayout {
	int output_width = 0;
	int output_height = 0;
	int overlay_width = 0;
	int overlay_height = 0;
	int overlay_x = 0;
	int overlay_y = 0;
	double overlay_rotation = 0.0;
};

struct TimingStats {
	int frames = 0;
	double decode_ms = 0.0;
	double upload_ms = 0.0;
	double composite_ms = 0.0;
	double total_ms = 0.0;

	void Print(const std::string& name) const {
		std::cout << "\n[" << name << "]\n";
		std::cout << "frames=" << frames << "\n";
		std::cout << std::fixed << std::setprecision(3)
				  << "decode_ms_total=" << decode_ms
				  << " decode_ms_avg=" << (frames ? decode_ms / frames : 0.0) << "\n"
				  << "upload_ms_total=" << upload_ms
				  << " upload_ms_avg=" << (frames ? upload_ms / frames : 0.0) << "\n"
				  << "composite_ms_total=" << composite_ms
				  << " composite_ms_avg=" << (frames ? composite_ms / frames : 0.0) << "\n"
				  << "total_ms=" << total_ms
				  << " fps=" << (total_ms > 0.0 ? (frames * 1000.0) / total_ms : 0.0) << "\n";
	}
};

enum class DecodeKind {
	CpuReader,
	Software,
	Cuda,
	Vaapi,
	Vulkan
};

struct BenchmarkResult {
	std::string name;
	std::string decode_backend;
	std::string composite_backend;
	std::string note;
	bool hw_decode_requested = false;
	bool hw_decode_used = false;
	bool readback_to_cpu = false;
	bool upload_to_vulkan = false;
	bool skipped = false;
	TimingStats stats;
};

struct VulkanSummary {
	uint32_t api_version = VK_API_VERSION_1_0;
	std::vector<std::string> device_names;
};

struct BackendAvailability {
	bool vaapi = false;
	bool cuda = false;
	bool vulkan_runtime = false;
};

static std::string AvError(int errnum) {
	char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
	av_make_error_string(buffer, sizeof(buffer), errnum);
	return std::string(buffer);
}

static void CheckAv(int errnum, const std::string& context) {
	if (errnum < 0) {
		throw std::runtime_error(context + ": " + AvError(errnum));
	}
}

static std::string Basename(const std::string& path) {
	const std::string::size_type slash = path.find_last_of('/');
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

static void DebugLog(const std::string& message) {
	(void) message;
}

static bool ShouldDumpFrame(const BenchmarkOptions& options, int frame_number) {
	return !options.dump_dir.empty() && options.dump_every > 0 &&
		   frame_number > 0 && ((frame_number - 1) % options.dump_every == 0);
}

static std::filesystem::path DumpFramePath(const BenchmarkOptions& options,
										   const std::string& row_name,
										   int frame_number) {
	std::filesystem::path row_dir = std::filesystem::path(options.dump_dir) / row_name;
	std::filesystem::create_directories(row_dir);
	std::ostringstream filename;
	filename << "frame-" << std::setw(6) << std::setfill('0') << frame_number << ".png";
	return row_dir / filename.str();
}

static void SaveDumpImage(const BenchmarkOptions& options,
						  const std::string& row_name,
						  int frame_number,
						  const QImage& image) {
	if (!ShouldDumpFrame(options, frame_number))
		return;
	const std::filesystem::path output_path = DumpFramePath(options, row_name, frame_number);
	if (!image.save(QString::fromStdString(output_path.string())))
		throw std::runtime_error("Unable to save dump image: " + output_path.string());
}

class GenericDecodeReader;
class RgbaFrameConverter;
static QImage QImageFromRgbaFrame(const AVFrame* frame);

static VulkanSummary QueryVulkanSummary() {
	VulkanSummary summary;
	auto enumerate_instance_version =
		reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
	if (enumerate_instance_version) {
		enumerate_instance_version(&summary.api_version);
	}

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "openshot-vulkan-benchmark";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "none";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = summary.api_version;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	VkInstance instance = VK_NULL_HANDLE;
	if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
		return summary;
	}

	uint32_t count = 0;
	if (vkEnumeratePhysicalDevices(instance, &count, nullptr) == VK_SUCCESS && count > 0) {
		std::vector<VkPhysicalDevice> devices(count);
		if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) == VK_SUCCESS) {
			for (VkPhysicalDevice device : devices) {
				VkPhysicalDeviceProperties properties{};
				vkGetPhysicalDeviceProperties(device, &properties);
				summary.device_names.emplace_back(properties.deviceName);
			}
		}
	}

	vkDestroyInstance(instance, nullptr);
	return summary;
}

static void PrintVulkanSummary(const VulkanSummary& summary) {
	std::cout << "Vulkan API version: "
			  << VK_VERSION_MAJOR(summary.api_version) << "."
			  << VK_VERSION_MINOR(summary.api_version) << "."
			  << VK_VERSION_PATCH(summary.api_version) << "\n";
	if (summary.device_names.empty()) {
		std::cout << "Vulkan devices: none detected\n";
		return;
	}

	std::cout << "Vulkan devices:\n";
	for (const std::string& name : summary.device_names) {
		std::cout << "  - " << name << "\n";
	}
}

static bool CanCreateHwDevice(AVHWDeviceType type, const char* device_name = nullptr) {
	AVBufferRef* device = nullptr;
	const int result = av_hwdevice_ctx_create(&device, type, device_name, nullptr, 0);
	if (device)
		av_buffer_unref(&device);
	return result >= 0;
}

static BackendAvailability ProbeBackends(const BenchmarkOptions& options) {
	BackendAvailability availability;
	availability.vaapi = CanCreateHwDevice(AV_HWDEVICE_TYPE_VAAPI, options.vaapi_device.c_str());
	availability.cuda = CanCreateHwDevice(AV_HWDEVICE_TYPE_CUDA, nullptr);
	availability.vulkan_runtime = CanCreateHwDevice(AV_HWDEVICE_TYPE_VULKAN, nullptr);
	return availability;
}

static RenderLayout ComputeRenderLayout(const BenchmarkOptions& options, int input_width, int input_height) {
	RenderLayout layout;
	if (options.output_width > 0 && options.output_height > 0) {
		layout.output_width = options.output_width;
		layout.output_height = options.output_height;
	} else if (options.mode == "export") {
		layout.output_width = input_width;
		layout.output_height = input_height;
	} else {
		const double width_scale = static_cast<double>(options.preview_width) / std::max(1, input_width);
		const double height_scale = static_cast<double>(options.preview_height) / std::max(1, input_height);
		const double scale = std::min(width_scale, height_scale);
		layout.output_width = std::max(1, static_cast<int>(std::lround(input_width * scale)));
		layout.output_height = std::max(1, static_cast<int>(std::lround(input_height * scale)));
	}

	layout.overlay_width = std::max(48, static_cast<int>(std::lround(layout.output_width * 0.28)));
	layout.overlay_height = std::max(27, static_cast<int>(std::lround(layout.overlay_width * 9.0 / 16.0)));
	layout.overlay_x = std::max(0, layout.output_width - layout.overlay_width - 24);
	layout.overlay_y = 24;
	layout.overlay_rotation = options.overlay_rotation;
	return layout;
}

static QImage LoadOverlayImage(const BenchmarkOptions& options) {
	QImage overlay(QString::fromStdString(options.overlay_path));
	if (overlay.isNull()) {
		throw std::runtime_error("Unable to load overlay image: " + options.overlay_path);
	}

	return overlay.convertToFormat(QImage::Format_RGBA8888);
}

static QImage PrepareOverlayImage(const BenchmarkOptions& options, const RenderLayout& layout, const QImage& overlay_base) {
	QImage scaled = overlay_base.scaled(
		layout.overlay_width,
		layout.overlay_height,
		Qt::IgnoreAspectRatio,
		Qt::SmoothTransformation);

	QTransform rotation;
	rotation.rotate(layout.overlay_rotation);
	QImage rgba = scaled.transformed(rotation, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
	QPainter alpha_painter(&rgba);
	alpha_painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	alpha_painter.fillRect(rgba.rect(), QColor(255, 255, 255,
					  static_cast<int>(std::clamp(options.overlay_alpha, 0.0, 1.0) * 255.0)));
	alpha_painter.end();
	return rgba;
}

static TimingStats RunCpuBenchmark(const BenchmarkOptions& options, const QImage& overlay_image) {
	TimingStats stats;
	const auto total_start = Clock::now();

	FFmpegReader reader(options.video_path);
	reader.Open();

	const int64_t frame_limit = std::min<int64_t>(options.max_frames, reader.info.video_length);
	if (frame_limit <= 0) {
		reader.Close();
		return stats;
	}

	RenderLayout layout;
	QImage prepared_overlay;
	bool overlay_ready = false;
	for (int64_t frame_number = 1; frame_number <= frame_limit; ++frame_number) {
		const auto decode_start = Clock::now();
		std::shared_ptr<Frame> source_frame = reader.GetFrame(frame_number);
		const auto decode_end = Clock::now();
		stats.decode_ms += std::chrono::duration<double, std::milli>(decode_end - decode_start).count();

		const auto composite_start = Clock::now();
		const std::shared_ptr<QImage> source_image = source_frame->GetImage();
		if (!overlay_ready) {
			layout = ComputeRenderLayout(options, source_image->width(), source_image->height());
			prepared_overlay = PrepareOverlayImage(options, layout, overlay_image);
			overlay_ready = true;
		}
		QImage output(layout.output_width, layout.output_height, QImage::Format_RGBA8888_Premultiplied);
		output.fill(QColor("#101418"));

		QPainter painter(&output);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
		painter.setRenderHint(QPainter::Antialiasing, true);

		painter.drawImage(output.rect(), *source_image);
		painter.setOpacity(1.0);
		painter.drawImage(layout.overlay_x, layout.overlay_y, prepared_overlay);
		painter.end();
		SaveDumpImage(options, "CPU->CPU", static_cast<int>(frame_number), output);

		const auto composite_end = Clock::now();
		stats.composite_ms += std::chrono::duration<double, std::milli>(composite_end - composite_start).count();
		stats.frames++;
	}

	reader.Close();
	stats.total_ms = std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
	return stats;
}

static TimingStats RunCpuCompositeBenchmark(const BenchmarkOptions& options, const QImage& overlay_image,
											 DecodeKind decode_kind, bool& used_hw_decode);

class GenericDecodeReader {
public:
	GenericDecodeReader(const BenchmarkOptions& options, DecodeKind decode_kind)
		: options_(options),
		  decode_kind_(decode_kind),
		  format_context_(nullptr),
		  codec_context_(nullptr),
		  codec_(nullptr),
		  stream_(nullptr),
		  hw_device_context_(nullptr),
		  packet_(),
		  draining_(false),
		  video_stream_index_(-1),
		  frame_counter_(0),
		  requested_hw_decode_(decode_kind == DecodeKind::Cuda || decode_kind == DecodeKind::Vaapi || decode_kind == DecodeKind::Vulkan),
		  decoded_hw_frames_(false) {}

	~GenericDecodeReader() { Close(); }

	void Open() {
		CheckAv(avformat_open_input(&format_context_, options_.video_path.c_str(), nullptr, nullptr),
				"avformat_open_input");
		CheckAv(avformat_find_stream_info(format_context_, nullptr), "avformat_find_stream_info");

		video_stream_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		CheckAv(video_stream_index_, "av_find_best_stream");

		stream_ = format_context_->streams[video_stream_index_];
		codec_ = avcodec_find_decoder(stream_->codecpar->codec_id);
		if (!codec_) {
			throw std::runtime_error("Unable to find decoder for input video stream");
		}

		codec_context_ = avcodec_alloc_context3(codec_);
		if (!codec_context_) {
			throw std::runtime_error("Unable to allocate codec context");
		}

		CheckAv(avcodec_parameters_to_context(codec_context_, stream_->codecpar),
				"avcodec_parameters_to_context");
		codec_context_->thread_count = std::min(8, std::max(1, static_cast<int>(std::thread::hardware_concurrency())));
		if (decode_kind_ == DecodeKind::Vulkan) {
			// Frame-threaded decode has shown stalls with Vulkan hw surfaces on some drivers.
			// Keep multi-threading enabled via slice threads while disabling frame threading.
			codec_context_->thread_type &= ~FF_THREAD_FRAME;
			if (codec_context_->thread_type == 0)
				codec_context_->thread_type = FF_THREAD_SLICE;
		}
		codec_context_->pkt_timebase = stream_->time_base;

		AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_NONE;
		const char* device_name = nullptr;
		if (decode_kind_ == DecodeKind::Cuda) {
			hw_device_type = AV_HWDEVICE_TYPE_CUDA;
		} else if (decode_kind_ == DecodeKind::Vaapi) {
			hw_device_type = AV_HWDEVICE_TYPE_VAAPI;
			device_name = options_.vaapi_device.c_str();
		} else if (decode_kind_ == DecodeKind::Vulkan) {
			hw_device_type = AV_HWDEVICE_TYPE_VULKAN;
		}

		if (hw_device_type != AV_HWDEVICE_TYPE_NONE &&
			av_hwdevice_ctx_create(&hw_device_context_, hw_device_type, device_name, nullptr, 0) >= 0) {
			codec_context_->opaque = this;
			codec_context_->get_format = &GenericDecodeReader::SelectPixelFormat;
			codec_context_->hw_device_ctx = av_buffer_ref(hw_device_context_);
		}

		CheckAv(avcodec_open2(codec_context_, codec_, nullptr), "avcodec_open2");
	}

	void Close() {
		if (codec_context_) {
			avcodec_free_context(&codec_context_);
			codec_context_ = nullptr;
		}
		if (format_context_) {
			avformat_close_input(&format_context_);
			format_context_ = nullptr;
		}
		if (hw_device_context_) {
			av_buffer_unref(&hw_device_context_);
		}
		draining_ = false;
		video_stream_index_ = -1;
		stream_ = nullptr;
	}

	double Fps() const {
		const AVRational avg = stream_->avg_frame_rate.num > 0 ? stream_->avg_frame_rate : stream_->r_frame_rate;
		if (avg.num <= 0 || avg.den <= 0)
			return 30.0;
		return av_q2d(avg);
	}

	int Width() const { return codec_context_ ? codec_context_->width : 0; }
	int Height() const { return codec_context_ ? codec_context_->height : 0; }
	bool RequestedHwDecode() const { return requested_hw_decode_; }
	bool UsingHwDecode() const { return decoded_hw_frames_; }
	AVBufferRef* DeviceContext() const { return hw_device_context_; }

	bool NextFrame(ScopedFrame& frame, double& decode_ms) {
		const auto start = Clock::now();
		AVFrame* decoded = av_frame_alloc();
		if (!decoded) {
			throw std::runtime_error("Unable to allocate decode frame");
		}

		while (true) {
			const int receive_result = avcodec_receive_frame(codec_context_, decoded);
			if (receive_result == 0) {
				const AVPixFmtDescriptor* desc =
					av_pix_fmt_desc_get(static_cast<AVPixelFormat>(decoded->format));
				if ((desc && (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) || decoded->hw_frames_ctx)
					decoded_hw_frames_ = true;
				decoded->pts = frame_counter_++;
				frame = ScopedFrame(decoded);
				decode_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
				return true;
			}
			if (receive_result == AVERROR_EOF) {
				av_frame_free(&decoded);
				decode_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
				return false;
			}
			if (receive_result != AVERROR(EAGAIN)) {
				const std::string err = AvError(receive_result);
				av_frame_free(&decoded);
				throw std::runtime_error("avcodec_receive_frame failed: " + err);
			}

			int send_result = 0;
			if (!draining_) {
				while (true) {
					send_result = av_read_frame(format_context_, packet_.get());
					if (send_result < 0) {
						draining_ = true;
						send_result = avcodec_send_packet(codec_context_, nullptr);
						break;
					}
					if (packet_.get()->stream_index != video_stream_index_) {
						av_packet_unref(packet_.get());
						continue;
					}

					send_result = avcodec_send_packet(codec_context_, packet_.get());
					av_packet_unref(packet_.get());
					break;
				}
			} else {
				send_result = avcodec_send_packet(codec_context_, nullptr);
			}

			if (send_result == AVERROR(EAGAIN))
				continue;
			if (send_result < 0 && send_result != AVERROR_EOF) {
				const std::string err = AvError(send_result);
				av_frame_free(&decoded);
				throw std::runtime_error("avcodec_send_packet failed: " + err);
			}
		}
	}

private:
	static enum AVPixelFormat SelectPixelFormat(AVCodecContext* codec_context, const enum AVPixelFormat* pix_fmts) {
		const auto* self = static_cast<GenericDecodeReader*>(codec_context->opaque);
		const AVPixelFormat preferred_hw =
			self && self->decode_kind_ == DecodeKind::Cuda ? AV_PIX_FMT_CUDA :
			self && self->decode_kind_ == DecodeKind::Vaapi ? AV_PIX_FMT_VAAPI :
			self && self->decode_kind_ == DecodeKind::Vulkan ? AV_PIX_FMT_VULKAN :
			AV_PIX_FMT_NONE;
		for (const enum AVPixelFormat* format = pix_fmts; *format != AV_PIX_FMT_NONE; ++format) {
			if (*format == preferred_hw)
				return *format;
		}
		for (const enum AVPixelFormat* format = pix_fmts; *format != AV_PIX_FMT_NONE; ++format) {
			const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*format);
			if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
				return *format;
		}
		return pix_fmts[0];
	}

	const BenchmarkOptions& options_;
	DecodeKind decode_kind_;
	AVFormatContext* format_context_;
	AVCodecContext* codec_context_;
	const AVCodec* codec_;
	AVStream* stream_;
	AVBufferRef* hw_device_context_;
	ScopedPacket packet_;
	bool draining_;
	int video_stream_index_;
	int64_t frame_counter_;
	bool requested_hw_decode_;
	bool decoded_hw_frames_;
};

class RgbaFrameConverter {
public:
	RgbaFrameConverter() : sws_context_(nullptr) {}
	~RgbaFrameConverter() {
		if (sws_context_)
			sws_freeContext(sws_context_);
	}

	ScopedFrame Convert(const AVFrame* input_frame) {
		ScopedFrame software_frame = DownloadIfNeeded(input_frame);
		if (software_frame.get()->format == AV_PIX_FMT_RGBA)
			return software_frame;

		sws_context_ = sws_getCachedContext(
			sws_context_,
			software_frame.get()->width,
			software_frame.get()->height,
			static_cast<AVPixelFormat>(software_frame.get()->format),
			software_frame.get()->width,
			software_frame.get()->height,
			AV_PIX_FMT_RGBA,
			SWS_BILINEAR,
			nullptr,
			nullptr,
			nullptr);
		if (!sws_context_)
			throw std::runtime_error("sws_getCachedContext failed");

		ScopedFrame rgba_frame;
		rgba_frame.get()->format = AV_PIX_FMT_RGBA;
		rgba_frame.get()->width = software_frame.get()->width;
		rgba_frame.get()->height = software_frame.get()->height;
		CheckAv(av_frame_get_buffer(rgba_frame.get(), 32), "av_frame_get_buffer rgba");
		CheckAv(av_frame_copy_props(rgba_frame.get(), software_frame.get()), "av_frame_copy_props rgba");
		CheckAv(av_frame_make_writable(rgba_frame.get()), "av_frame_make_writable rgba");

		sws_scale(
			sws_context_,
			software_frame.get()->data,
			software_frame.get()->linesize,
			0,
			software_frame.get()->height,
			rgba_frame.get()->data,
			rgba_frame.get()->linesize);
		return rgba_frame;
	}

private:
	ScopedFrame DownloadIfNeeded(const AVFrame* input_frame) {
		const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(input_frame->format));
		if (!desc || !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
			ScopedFrame clone(av_frame_clone(input_frame));
			if (!clone)
				throw std::runtime_error("Unable to clone software input frame");
			return clone;
		}

		if (!input_frame->hw_frames_ctx)
			throw std::runtime_error("Hardware frame is missing hw_frames_ctx");

		auto* frames_context = reinterpret_cast<AVHWFramesContext*>(input_frame->hw_frames_ctx->data);
		ScopedFrame software_frame;
		software_frame.get()->format = frames_context->sw_format;
		software_frame.get()->width = input_frame->width;
		software_frame.get()->height = input_frame->height;
		CheckAv(av_frame_get_buffer(software_frame.get(), 32), "av_frame_get_buffer transfer");
		CheckAv(av_hwframe_transfer_data(software_frame.get(), input_frame, 0), "av_hwframe_transfer_data download");
		CheckAv(av_frame_copy_props(software_frame.get(), input_frame), "av_frame_copy_props transfer");
		return software_frame;
	}

	SwsContext* sws_context_;
};

class RgbaFrameResizer {
public:
	RgbaFrameResizer() : sws_context_(nullptr) {}
	~RgbaFrameResizer() {
		if (sws_context_)
			sws_freeContext(sws_context_);
	}

	ScopedFrame Resize(const AVFrame* input_frame, int width, int height) {
		if (!input_frame || input_frame->format != AV_PIX_FMT_RGBA)
			throw std::runtime_error("Resize expects an RGBA software frame");
		if (input_frame->width == width && input_frame->height == height) {
			ScopedFrame clone(av_frame_clone(input_frame));
			if (!clone)
				throw std::runtime_error("Unable to clone RGBA input frame");
			return clone;
		}

		sws_context_ = sws_getCachedContext(
			sws_context_,
			input_frame->width,
			input_frame->height,
			AV_PIX_FMT_RGBA,
			width,
			height,
			AV_PIX_FMT_RGBA,
			SWS_BILINEAR,
			nullptr,
			nullptr,
			nullptr);
		if (!sws_context_)
			throw std::runtime_error("sws_getCachedContext resize failed");

		ScopedFrame output_frame;
		output_frame.get()->format = AV_PIX_FMT_RGBA;
		output_frame.get()->width = width;
		output_frame.get()->height = height;
		CheckAv(av_frame_get_buffer(output_frame.get(), 32), "av_frame_get_buffer resize");
		CheckAv(av_frame_copy_props(output_frame.get(), input_frame), "av_frame_copy_props resize");
		CheckAv(av_frame_make_writable(output_frame.get()), "av_frame_make_writable resize");
		sws_scale(
			sws_context_,
			input_frame->data,
			input_frame->linesize,
			0,
			input_frame->height,
			output_frame.get()->data,
			output_frame.get()->linesize);
		return output_frame;
	}

private:
	SwsContext* sws_context_;
};

static QImage QImageFromRgbaFrame(const AVFrame* frame) {
	if (!frame || frame->format != AV_PIX_FMT_RGBA)
		throw std::runtime_error("Expected an RGBA software frame");

	QImage image(frame->width, frame->height, QImage::Format_RGBA8888);
	for (int y = 0; y < frame->height; ++y) {
		std::memcpy(image.scanLine(y),
					frame->data[0] + y * frame->linesize[0],
					static_cast<size_t>(frame->width) * 4);
	}
	return image;
}

static void ForceOpaqueAlpha(AVFrame* frame) {
	if (!frame || frame->format != AV_PIX_FMT_RGBA)
		throw std::runtime_error("ForceOpaqueAlpha expects an RGBA software frame");
	CheckAv(av_frame_make_writable(frame), "av_frame_make_writable opaque alpha");
	for (int y = 0; y < frame->height; ++y) {
		uint8_t* row = frame->data[0] + y * frame->linesize[0];
		for (int x = 0; x < frame->width; ++x) {
			row[x * 4 + 3] = 255;
		}
	}
}

static TimingStats RunCpuCompositeBenchmark(const BenchmarkOptions& options, const QImage& overlay_image,
											 DecodeKind decode_kind, bool& used_hw_decode) {
	TimingStats stats;
	const auto total_start = Clock::now();
	GenericDecodeReader decoder(options, decode_kind);
	decoder.Open();
	used_hw_decode = false;

	RgbaFrameConverter converter;
	RenderLayout layout;
	QImage prepared_overlay;
	bool overlay_ready = false;
	for (int frame_index = 0; frame_index < options.max_frames; ++frame_index) {
		ScopedFrame input_frame;
		double decode_ms = 0.0;
		if (!decoder.NextFrame(input_frame, decode_ms))
			break;
		stats.decode_ms += decode_ms;
		used_hw_decode = used_hw_decode || decoder.UsingHwDecode();

		const auto composite_start = Clock::now();
		ScopedFrame rgba_frame = converter.Convert(input_frame.get());
		QImage source_image = QImageFromRgbaFrame(rgba_frame.get());
		if (!overlay_ready) {
			layout = ComputeRenderLayout(options, source_image.width(), source_image.height());
			prepared_overlay = PrepareOverlayImage(options, layout, overlay_image);
			overlay_ready = true;
		}
		QImage output(layout.output_width, layout.output_height, QImage::Format_RGBA8888_Premultiplied);
		output.fill(QColor("#101418"));

		QPainter painter(&output);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.drawImage(output.rect(), source_image);
		painter.setOpacity(1.0);
		painter.drawImage(layout.overlay_x, layout.overlay_y, prepared_overlay);
		painter.end();
		const std::string row_name =
			decode_kind == DecodeKind::Cuda ? "CUDA->CPU" :
			decode_kind == DecodeKind::Vaapi ? "VAAPI->CPU" :
			decode_kind == DecodeKind::Vulkan ? "Vulkan->CPU" :
			"CPU->CPU";
		SaveDumpImage(options, row_name, frame_index + 1, output);
		const auto composite_end = Clock::now();

		stats.composite_ms += std::chrono::duration<double, std::milli>(composite_end - composite_start).count();
		stats.frames++;
	}

	decoder.Close();
	stats.total_ms = std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
	return stats;
}

static ScopedFrame ConvertQImageToAvFrame(const QImage& image) {
	QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
	ScopedFrame frame;
	if (!frame) {
		throw std::runtime_error("Unable to allocate overlay frame");
	}

	frame.get()->format = AV_PIX_FMT_RGBA;
	frame.get()->width = rgba.width();
	frame.get()->height = rgba.height();
	CheckAv(av_frame_get_buffer(frame.get(), 32), "av_frame_get_buffer");
	CheckAv(av_frame_make_writable(frame.get()), "av_frame_make_writable");

	for (int y = 0; y < rgba.height(); ++y) {
		std::memcpy(frame.get()->data[0] + y * frame.get()->linesize[0],
					rgba.constScanLine(y),
					static_cast<size_t>(rgba.width()) * 4);
	}
	return frame;
}

struct FilterGraph {
	AVFilterGraph* graph = nullptr;
	AVFilterContext* main_source = nullptr;
	AVFilterContext* overlay_source = nullptr;
	AVFilterContext* sink = nullptr;

	~FilterGraph() {
		if (graph)
			avfilter_graph_free(&graph);
	}
};

static FilterGraph BuildVulkanFilterGraph(const RenderLayout& layout,
										  int overlay_width,
										  int overlay_height,
										  AVBufferRef* vulkan_device_context,
										  double fps,
										  int input_width,
										  int input_height) {
	FilterGraph filter_graph;
	filter_graph.graph = avfilter_graph_alloc();
	if (!filter_graph.graph)
		throw std::runtime_error("Unable to allocate filter graph");

	const AVFilter* buffer_filter = avfilter_get_by_name("buffer");
	const AVFilter* hwupload_filter = avfilter_get_by_name("hwupload");
	const AVFilter* overlay_vulkan_filter = avfilter_get_by_name("overlay_vulkan");
	const AVFilter* sink_filter = avfilter_get_by_name("buffersink");
	if (!buffer_filter || !hwupload_filter || !overlay_vulkan_filter || !sink_filter) {
		throw std::runtime_error("Required FFmpeg Vulkan filters are not available");
	}

	auto make_buffer_args = [fps](int width, int height) {
		std::ostringstream args;
		args << "video_size=" << width << "x" << height
			 << ":pix_fmt=" << AV_PIX_FMT_RGBA
			 << ":time_base=1/" << std::max(1, static_cast<int>(std::lround(fps)))
			 << ":pixel_aspect=1/1";
		return args.str();
	};

	std::string main_args = make_buffer_args(input_width, input_height);
	CheckAv(avfilter_graph_create_filter(&filter_graph.main_source, buffer_filter, "main_in",
										 main_args.c_str(), nullptr, filter_graph.graph),
			"avfilter_graph_create_filter main source");
	std::string overlay_args = make_buffer_args(overlay_width, overlay_height);
	CheckAv(avfilter_graph_create_filter(&filter_graph.overlay_source, buffer_filter, "overlay_in",
										 overlay_args.c_str(), nullptr, filter_graph.graph),
			"avfilter_graph_create_filter overlay source");
	CheckAv(avfilter_graph_create_filter(&filter_graph.sink, sink_filter, "sink",
										 nullptr, nullptr, filter_graph.graph),
			"avfilter_graph_create_filter sink");
	AVFilterContext* main_hwupload = nullptr;
	AVFilterContext* overlay_hwupload = nullptr;
	AVFilterContext* overlay = nullptr;
	CheckAv(avfilter_graph_create_filter(&main_hwupload, hwupload_filter, "main_hwupload",
										 nullptr, nullptr, filter_graph.graph),
			"avfilter_graph_create_filter main hwupload");
	CheckAv(avfilter_graph_create_filter(&overlay_hwupload, hwupload_filter, "overlay_hwupload",
										 nullptr, nullptr, filter_graph.graph),
			"avfilter_graph_create_filter overlay hwupload");
	main_hwupload->hw_device_ctx = av_buffer_ref(vulkan_device_context);
	overlay_hwupload->hw_device_ctx = av_buffer_ref(vulkan_device_context);
	if (!main_hwupload->hw_device_ctx || !overlay_hwupload->hw_device_ctx)
		throw std::runtime_error("Unable to retain Vulkan device context for hwupload");

	std::ostringstream overlay_args_filter;
	overlay_args_filter << "x=" << layout.overlay_x
						<< ":y=" << layout.overlay_y;
	CheckAv(avfilter_graph_create_filter(&overlay, overlay_vulkan_filter, "overlay",
										 overlay_args_filter.str().c_str(), nullptr, filter_graph.graph),
			"avfilter_graph_create_filter overlay");

	CheckAv(avfilter_link(filter_graph.main_source, 0, main_hwupload, 0), "avfilter_link main src->hwupload");
	CheckAv(avfilter_link(main_hwupload, 0, overlay, 0), "avfilter_link main hwupload->overlay");
	CheckAv(avfilter_link(filter_graph.overlay_source, 0, overlay_hwupload, 0), "avfilter_link overlay src->hwupload");
	CheckAv(avfilter_link(overlay_hwupload, 0, overlay, 1), "avfilter_link overlay hwupload->overlay");
	CheckAv(avfilter_link(overlay, 0, filter_graph.sink, 0), "avfilter_link sink");
	CheckAv(avfilter_graph_config(filter_graph.graph, nullptr), "avfilter_graph_config");

	return filter_graph;
}

static FilterGraph BuildVulkanDirectTransformGraph(AVBufferRef* main_hw_frames_context,
												   double fps,
												   int input_width,
												   int input_height) {
	FilterGraph filter_graph;
	filter_graph.graph = avfilter_graph_alloc();
	if (!filter_graph.graph)
		throw std::runtime_error("Unable to allocate direct Vulkan filter graph");

	const AVFilter* buffer_filter = avfilter_get_by_name("buffer");
	const AVFilter* hflip_filter = avfilter_get_by_name("hflip_vulkan");
	const AVFilter* sink_filter = avfilter_get_by_name("buffersink");
	if (!buffer_filter || !hflip_filter || !sink_filter)
		throw std::runtime_error("Required direct Vulkan filters are not available");

	std::ostringstream args;
	args << "video_size=" << input_width << "x" << input_height
		 << ":pix_fmt=" << AV_PIX_FMT_VULKAN
		 << ":time_base=1/" << std::max(1, static_cast<int>(std::lround(fps)))
		 << ":pixel_aspect=1/1";
	CheckAv(avfilter_graph_create_filter(&filter_graph.main_source, buffer_filter, "main_in",
										 args.str().c_str(), nullptr, filter_graph.graph),
			"avfilter_graph_create_filter direct main source");
	CheckAv(avfilter_graph_create_filter(&filter_graph.sink, sink_filter, "sink",
										 nullptr, nullptr, filter_graph.graph),
			"avfilter_graph_create_filter direct sink");

	AVBufferSrcParameters* main_params = av_buffersrc_parameters_alloc();
	if (!main_params)
		throw std::runtime_error("Unable to allocate direct buffer source parameters");
	main_params->format = AV_PIX_FMT_VULKAN;
	main_params->width = input_width;
	main_params->height = input_height;
	main_params->time_base = AVRational{1, std::max(1, static_cast<int>(std::lround(fps)))};
	main_params->hw_frames_ctx = av_buffer_ref(main_hw_frames_context);
	CheckAv(av_buffersrc_parameters_set(filter_graph.main_source, main_params),
			"av_buffersrc_parameters_set direct main");
	av_free(main_params);

	AVFilterContext* transform = nullptr;
	CheckAv(avfilter_graph_create_filter(&transform, hflip_filter, "hflip",
										 nullptr, nullptr, filter_graph.graph),
			"avfilter_graph_create_filter direct hflip");
	CheckAv(avfilter_link(filter_graph.main_source, 0, transform, 0), "avfilter_link direct main");
	CheckAv(avfilter_link(transform, 0, filter_graph.sink, 0), "avfilter_link direct sink");
	CheckAv(avfilter_graph_config(filter_graph.graph, nullptr), "avfilter_graph_config direct");
	return filter_graph;
}

static std::vector<uint32_t> LoadSpirvFile(const std::string& path) {
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream.is_open())
		throw std::runtime_error("Unable to open SPIR-V shader: " + path);
	const std::streamsize size = stream.tellg();
	constexpr std::streamsize kMaxShaderBytes = 16 * 1024 * 1024;
	if (size <= 0 || size > kMaxShaderBytes || (size % 4) != 0)
		throw std::runtime_error("Invalid SPIR-V shader size: " + path);
	stream.seekg(0, std::ios::beg);
	std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
	const size_t byte_count = code.size() * sizeof(uint32_t);
	if (byte_count != static_cast<size_t>(size))
		throw std::runtime_error("SPIR-V shader size overflow: " + path);
	char* byte_ptr = reinterpret_cast<char*>(code.data());
	if (!stream.read(byte_ptr, static_cast<std::streamsize>(byte_count)) ||
		stream.gcount() != static_cast<std::streamsize>(byte_count))
		throw std::runtime_error("Unable to read SPIR-V shader: " + path);
	return code;
}

static void CheckVk(VkResult result, const std::string& context) {
	if (result != VK_SUCCESS)
		throw std::runtime_error(context + ": VkResult=" + std::to_string(static_cast<int>(result)));
}

class DirectVulkanPreviewCompositor {
public:
	static constexpr uint64_t kQueueWaitTimeoutNs = 10ull * 1000ull * 1000ull * 1000ull;

	DirectVulkanPreviewCompositor(const BenchmarkOptions& options,
								  const QImage& overlay_image,
								  AVBufferRef* hw_frames_context)
		: options_(options),
		  frames_context_(reinterpret_cast<AVHWFramesContext*>(hw_frames_context->data)),
		  vulkan_frames_context_(reinterpret_cast<AVVulkanFramesContext*>(frames_context_->hwctx)),
		  device_context_(frames_context_->device_ctx),
		  vulkan_device_context_(reinterpret_cast<AVVulkanDeviceContext*>(device_context_->hwctx)),
		  device_(vulkan_device_context_->act_dev),
		  physical_device_(vulkan_device_context_->phys_dev) {
		if (frames_context_->sw_format != AV_PIX_FMT_YUV420P &&
			frames_context_->sw_format != AV_PIX_FMT_NV12)
			throw std::runtime_error("Direct Vulkan compositor currently supports only yuv420p or nv12 decode surfaces");
		chroma_mode_ = frames_context_->sw_format == AV_PIX_FMT_NV12 ? 1 : 0;

		layout_ = ComputeRenderLayout(options_, frames_context_->width, frames_context_->height);
		prepared_overlay_ = PrepareOverlayImage(options_, layout_, overlay_image);
		output_width_ = layout_.output_width;
		output_height_ = layout_.output_height;
		compute_queue_family_ = vulkan_device_context_->queue_family_comp_index;
		if (compute_queue_family_ < 0)
			throw std::runtime_error("No Vulkan compute queue family available");

		vkGetDeviceQueue(device_, static_cast<uint32_t>(compute_queue_family_), 0, &compute_queue_);
		DebugLog("direct compositor: create command pool");
		CreateCommandPool();
		DebugLog("direct compositor: create descriptor set layout");
		CreateDescriptorSetLayout();
		DebugLog("direct compositor: create pipeline layout");
		CreatePipelineLayout();
		DebugLog("direct compositor: create descriptor pool");
		CreateDescriptorPool();
		DebugLog("direct compositor: create sampler");
		CreateSampler();
		DebugLog("direct compositor: create output image");
		CreateOutputImage();
		DebugLog("direct compositor: create overlay image");
		CreateOverlayImage();
		DebugLog("direct compositor: create pipeline");
		CreatePipeline();
		DebugLog("direct compositor: allocate descriptor set");
		AllocateDescriptorSet();
	}

	DirectVulkanPreviewCompositor(const BenchmarkOptions& options,
								  const QImage& overlay_image,
								  AVBufferRef* device_context_ref,
								  int input_width,
								  int input_height)
		: options_(options),
		  device_context_(reinterpret_cast<AVHWDeviceContext*>(device_context_ref->data)),
		  vulkan_device_context_(reinterpret_cast<AVVulkanDeviceContext*>(device_context_->hwctx)),
		  device_(vulkan_device_context_->act_dev),
		  physical_device_(vulkan_device_context_->phys_dev),
		  software_input_(true) {
		chroma_mode_ = 2;
		layout_ = ComputeRenderLayout(options_, input_width, input_height);
		prepared_overlay_ = PrepareOverlayImage(options_, layout_, overlay_image);
		output_width_ = layout_.output_width;
		output_height_ = layout_.output_height;
		compute_queue_family_ = vulkan_device_context_->queue_family_comp_index;
		if (compute_queue_family_ < 0)
			throw std::runtime_error("No Vulkan compute queue family available");

		vkGetDeviceQueue(device_, static_cast<uint32_t>(compute_queue_family_), 0, &compute_queue_);
		CreateCommandPool();
		CreateDescriptorSetLayout();
		CreatePipelineLayout();
		CreateDescriptorPool();
		CreateSampler();
		CreateOutputImage();
		CreateOverlayImage();
		CreateSourceImage(input_width, input_height);
		CreatePipeline();
		AllocateDescriptorSet();
	}

	~DirectVulkanPreviewCompositor() noexcept {
		if (device_ == VK_NULL_HANDLE)
			return;
		try {
			WaitForPendingSubmission("compositor shutdown");
			vkDeviceWaitIdle(device_);
		} catch (const std::exception& e) {
			DebugLog(std::string("direct compositor: shutdown cleanup failed: ") + e.what());
		} catch (...) {
			DebugLog("direct compositor: shutdown cleanup failed with unknown exception");
		}
		if (descriptor_pool_ != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
		if (pipeline_ != VK_NULL_HANDLE)
			vkDestroyPipeline(device_, pipeline_, nullptr);
		if (pipeline_layout_ != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
		if (descriptor_set_layout_ != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
		if (sampler_ != VK_NULL_HANDLE)
			vkDestroySampler(device_, sampler_, nullptr);
		if (overlay_view_ != VK_NULL_HANDLE)
			vkDestroyImageView(device_, overlay_view_, nullptr);
		if (overlay_image_ != VK_NULL_HANDLE)
			vkDestroyImage(device_, overlay_image_, nullptr);
		if (overlay_memory_ != VK_NULL_HANDLE)
			vkFreeMemory(device_, overlay_memory_, nullptr);
		if (source_view_ != VK_NULL_HANDLE)
			vkDestroyImageView(device_, source_view_, nullptr);
		if (source_image_ != VK_NULL_HANDLE)
			vkDestroyImage(device_, source_image_, nullptr);
		if (source_memory_ != VK_NULL_HANDLE)
			vkFreeMemory(device_, source_memory_, nullptr);
		if (output_view_ != VK_NULL_HANDLE)
			vkDestroyImageView(device_, output_view_, nullptr);
		if (output_image_ != VK_NULL_HANDLE)
			vkDestroyImage(device_, output_image_, nullptr);
		if (output_memory_ != VK_NULL_HANDLE)
			vkFreeMemory(device_, output_memory_, nullptr);
		if (command_pool_ != VK_NULL_HANDLE)
			vkDestroyCommandPool(device_, command_pool_, nullptr);
	}

	struct PushConstantBlock {
		int output_width;
		int output_height;
		int overlay_x;
		int overlay_y;
		int overlay_width;
		int overlay_height;
		int chroma_mode;
	};

	void Composite(const AVFrame* frame) {
		Submit(frame);
		WaitForPendingSubmission("direct compositor");
	}

	void Submit(const AVFrame* frame) {
		if (pending_submission_)
			throw std::runtime_error("Direct Vulkan compositor submission called while previous frame is still pending");
		if (software_input_)
			throw std::runtime_error("Composite(AVFrame) called on software-input compositor");
		auto* vk_frame = reinterpret_cast<AVVkFrame*>(frame->data[0]);
		if (!vk_frame)
			throw std::runtime_error("Missing AVVkFrame on direct Vulkan frame");
		DebugLog("direct compositor: begin frame");

		std::array<VkImageView, 3> video_views{};
		if (frames_context_->sw_format == AV_PIX_FMT_NV12) {
			DebugLog("direct compositor: create NV12 views");
			DebugLog("direct compositor: img0=" + std::to_string(reinterpret_cast<uintptr_t>(vk_frame->img[0])) +
					 " img1=" + std::to_string(reinterpret_cast<uintptr_t>(vk_frame->img[1])) +
					 " fmt0=" + std::to_string(static_cast<int>(vulkan_frames_context_->format[0])) +
					 " fmt1=" + std::to_string(static_cast<int>(vulkan_frames_context_->format[1])));
			video_views[0] = CreatePlaneView(vk_frame->img[0], VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_PLANE_0_BIT);
			video_views[1] = CreatePlaneView(vk_frame->img[0], VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_PLANE_1_BIT);
			video_views[2] = CreatePlaneView(vk_frame->img[0], VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_PLANE_1_BIT);
		} else {
			DebugLog("direct compositor: create planar views");
			for (int i = 0; i < 3; ++i)
				video_views[i] = CreatePlaneView(vk_frame->img[i], vulkan_frames_context_->format[i], VK_IMAGE_ASPECT_COLOR_BIT);
		}

		UpdateDescriptorSet(video_views);
		DebugLog("direct compositor: descriptor set updated");
		const std::array<VkImageLayout, 3> wait_layouts = {
			vk_frame->layout[0],
			vk_frame->layout[1],
			frames_context_->sw_format == AV_PIX_FMT_NV12 ? vk_frame->layout[1] : vk_frame->layout[2]
		};

		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool_;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VkCommandBuffer command_buffer = VK_NULL_HANDLE;
		CheckVk(vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer), "vkAllocateCommandBuffers");
		DebugLog("direct compositor: command buffer allocated");

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		CheckVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");
		DebugLog("direct compositor: command buffer begin");

		const int plane_count = frames_context_->sw_format == AV_PIX_FMT_NV12 ? 1 : 3;
		std::array<VkImageMemoryBarrier, 3> video_barriers{};
		for (int i = 0; i < plane_count; ++i) {
			video_barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			video_barriers[i].srcAccessMask = static_cast<VkAccessFlags>(vk_frame->access[i]);
			video_barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			video_barriers[i].oldLayout = wait_layouts[i];
			video_barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			video_barriers[i].srcQueueFamilyIndex = vk_frame->queue_family[i];
			video_barriers[i].dstQueueFamilyIndex =
				(vk_frame->queue_family[i] == VK_QUEUE_FAMILY_IGNORED) ? VK_QUEUE_FAMILY_IGNORED : static_cast<uint32_t>(compute_queue_family_);
			video_barriers[i].image = vk_frame->img[i];
			video_barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			video_barriers[i].subresourceRange.baseMipLevel = 0;
			video_barriers[i].subresourceRange.levelCount = 1;
			video_barriers[i].subresourceRange.baseArrayLayer = 0;
			video_barriers[i].subresourceRange.layerCount = 1;
		}

		vkCmdPipelineBarrier(
			command_buffer,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			static_cast<uint32_t>(plane_count),
			video_barriers.data());

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

		const PushConstantBlock push_constants{
			output_width_,
			output_height_,
			layout_.overlay_x,
			layout_.overlay_y,
			prepared_overlay_.width(),
			prepared_overlay_.height(),
			chroma_mode_
		};
		vkCmdPushConstants(command_buffer, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);
		vkCmdDispatch(command_buffer,
					  static_cast<uint32_t>((output_width_ + 15) / 16),
					  static_cast<uint32_t>((output_height_ + 15) / 16),
					  1);

		CheckVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");
		DebugLog("direct compositor: command buffer recorded");

		std::vector<VkSemaphore> wait_semaphores;
		std::vector<uint64_t> wait_values;
		std::vector<VkSemaphore> signal_semaphores;
		std::vector<uint64_t> signal_values;
		for (int i = 0; i < plane_count; ++i) {
			wait_semaphores.push_back(vk_frame->sem[i]);
			wait_values.push_back(vk_frame->sem_value[i]);
			signal_semaphores.push_back(vk_frame->sem[i]);
			signal_values.push_back(vk_frame->sem_value[i] + 1);
		}

		std::vector<VkPipelineStageFlags> wait_stages(wait_semaphores.size(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		VkTimelineSemaphoreSubmitInfo timeline_info{};
		timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		timeline_info.waitSemaphoreValueCount = static_cast<uint32_t>(wait_values.size());
		timeline_info.pWaitSemaphoreValues = wait_values.data();
		timeline_info.signalSemaphoreValueCount = static_cast<uint32_t>(signal_values.size());
		timeline_info.pSignalSemaphoreValues = signal_values.data();

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = &timeline_info;
		submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
		submit_info.pWaitSemaphores = wait_semaphores.data();
		submit_info.pWaitDstStageMask = wait_stages.data();
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
		submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
		submit_info.pSignalSemaphores = signal_semaphores.data();

		DebugLog("direct compositor: queue submit");
		pending_fence_ = Submit(submit_info, "direct compositor");
		pending_submission_ = true;
		pending_command_buffer_ = command_buffer;
		pending_video_views_ = video_views;

		for (int i = 0; i < plane_count; ++i) {
			vk_frame->layout[i] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vk_frame->access[i] = VK_ACCESS_SHADER_READ_BIT;
			vk_frame->queue_family[i] = static_cast<uint32_t>(compute_queue_family_);
			vk_frame->sem_value[i] += 1;
		}
	}

	void CompositeSoftwareRgba(const AVFrame* frame) {
		if (!software_input_)
			throw std::runtime_error("CompositeSoftwareRgba called on hardware-input compositor");
		if (!frame || frame->format != AV_PIX_FMT_RGBA)
			throw std::runtime_error("CompositeSoftwareRgba expects an RGBA frame");
		UploadSourceRgba(frame);
		std::array<VkImageView, 3> views = {source_view_, source_view_, source_view_};
		UpdateDescriptorSet(views);

		VkCommandBuffer command_buffer = BeginOneTimeCommands();
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);
		const PushConstantBlock push_constants{
			output_width_,
			output_height_,
			layout_.overlay_x,
			layout_.overlay_y,
			prepared_overlay_.width(),
			prepared_overlay_.height(),
			chroma_mode_
		};
		vkCmdPushConstants(command_buffer, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants), &push_constants);
		vkCmdDispatch(command_buffer,
					  static_cast<uint32_t>((output_width_ + 15) / 16),
					  static_cast<uint32_t>((output_height_ + 15) / 16),
					  1);
		EndOneTimeCommands(command_buffer);
	}

	void WaitForPendingSubmission(const std::string& context) {
		if (!pending_submission_)
			return;
		WaitForFence(pending_fence_, context);
		if (pending_fence_ != VK_NULL_HANDLE) {
			vkDestroyFence(device_, pending_fence_, nullptr);
			pending_fence_ = VK_NULL_HANDLE;
		}
		for (VkImageView view : pending_video_views_) {
			if (view != VK_NULL_HANDLE)
				vkDestroyImageView(device_, view, nullptr);
		}
		pending_video_views_.fill(VK_NULL_HANDLE);
		if (pending_command_buffer_ != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(device_, command_pool_, 1, &pending_command_buffer_);
			pending_command_buffer_ = VK_NULL_HANDLE;
		}
		pending_submission_ = false;
	}

	QImage ReadbackOutput() {
		const size_t byte_count = static_cast<size_t>(output_width_) * output_height_ * 4;

		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;

		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = byte_count;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		CheckVk(vkCreateBuffer(device_, &buffer_info, nullptr, &buffer), "vkCreateBuffer readback");

		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(device_, buffer, &requirements);
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = requirements.size;
		alloc_info.memoryTypeIndex = FindMemoryType(
			requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		CheckVk(vkAllocateMemory(device_, &alloc_info, nullptr, &memory), "vkAllocateMemory readback");
		CheckVk(vkBindBufferMemory(device_, buffer, memory, 0), "vkBindBufferMemory readback");

		VkCommandBuffer command_buffer = BeginOneTimeCommands();
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = output_image_;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = {static_cast<uint32_t>(output_width_), static_cast<uint32_t>(output_height_), 1};
		vkCmdCopyImageToBuffer(command_buffer, output_image_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

		VkImageMemoryBarrier restore{};
		restore.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		restore.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		restore.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		restore.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		restore.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		restore.image = output_image_;
		restore.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		restore.subresourceRange.levelCount = 1;
		restore.subresourceRange.layerCount = 1;
		restore.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		restore.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &restore);

		EndOneTimeCommands(command_buffer);

		void* mapped = nullptr;
		CheckVk(vkMapMemory(device_, memory, 0, byte_count, 0, &mapped), "vkMapMemory readback");
		QImage image(output_width_, output_height_, QImage::Format_RGBA8888);
		for (int y = 0; y < output_height_; ++y) {
			std::memcpy(image.scanLine(y),
						static_cast<const char*>(mapped) + static_cast<size_t>(y) * output_width_ * 4,
						static_cast<size_t>(output_width_) * 4);
		}
		vkUnmapMemory(device_, memory);
		vkDestroyBuffer(device_, buffer, nullptr);
		vkFreeMemory(device_, memory, nullptr);
		return image;
	}

private:
	uint32_t FindMemoryType(uint32_t type_bits, VkMemoryPropertyFlags properties) const {
		VkPhysicalDeviceMemoryProperties memory_properties{};
		vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
			if ((type_bits & (1u << i)) &&
				(memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}
		throw std::runtime_error("Unable to find matching Vulkan memory type");
	}

	void CreateCommandPool() {
		VkCommandPoolCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		info.queueFamilyIndex = static_cast<uint32_t>(compute_queue_family_);
		CheckVk(vkCreateCommandPool(device_, &info, nullptr, &command_pool_), "vkCreateCommandPool");
	}

	void CreateDescriptorSetLayout() {
		std::array<VkDescriptorSetLayoutBinding, 5> bindings{};
		for (uint32_t i = 0; i < 4; ++i) {
			bindings[i].binding = i;
			bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[i].descriptorCount = 1;
			bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		}
		bindings[4].binding = 4;
		bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bindings[4].descriptorCount = 1;
		bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		VkDescriptorSetLayoutCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = static_cast<uint32_t>(bindings.size());
		info.pBindings = bindings.data();
		CheckVk(vkCreateDescriptorSetLayout(device_, &info, nullptr, &descriptor_set_layout_),
				"vkCreateDescriptorSetLayout");
	}

	void CreatePipelineLayout() {
		VkPushConstantRange push_constant{};
		push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		push_constant.offset = 0;
		push_constant.size = sizeof(PushConstantBlock);

		VkPipelineLayoutCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount = 1;
		info.pSetLayouts = &descriptor_set_layout_;
		info.pushConstantRangeCount = 1;
		info.pPushConstantRanges = &push_constant;
		CheckVk(vkCreatePipelineLayout(device_, &info, nullptr, &pipeline_layout_), "vkCreatePipelineLayout");
	}

	void CreateDescriptorPool() {
		std::array<VkDescriptorPoolSize, 2> pool_sizes{};
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 4;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.maxSets = 1;
		info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
		info.pPoolSizes = pool_sizes.data();
		CheckVk(vkCreateDescriptorPool(device_, &info, nullptr, &descriptor_pool_), "vkCreateDescriptorPool");
	}

	void AllocateDescriptorSet() {
		VkDescriptorSetAllocateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool = descriptor_pool_;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &descriptor_set_layout_;
		CheckVk(vkAllocateDescriptorSets(device_, &info, &descriptor_set_), "vkAllocateDescriptorSets");
	}

	void CreateSampler() {
		VkSamplerCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.magFilter = VK_FILTER_LINEAR;
		info.minFilter = VK_FILTER_LINEAR;
		info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		info.maxLod = 1.0f;
		CheckVk(vkCreateSampler(device_, &info, nullptr, &sampler_), "vkCreateSampler");
	}

	void CreateOutputImage() {
		VkImageCreateInfo image_info{};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_info.extent = {static_cast<uint32_t>(output_width_), static_cast<uint32_t>(output_height_), 1};
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		CheckVk(vkCreateImage(device_, &image_info, nullptr, &output_image_), "vkCreateImage output");

		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device_, output_image_, &requirements);
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = requirements.size;
		alloc_info.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CheckVk(vkAllocateMemory(device_, &alloc_info, nullptr, &output_memory_), "vkAllocateMemory output");
		CheckVk(vkBindImageMemory(device_, output_image_, output_memory_, 0), "vkBindImageMemory output");
		output_view_ = CreatePlaneView(output_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void CreateSourceImage(int input_width, int input_height) {
		VkImageCreateInfo image_info{};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_info.extent = {static_cast<uint32_t>(input_width), static_cast<uint32_t>(input_height), 1};
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		CheckVk(vkCreateImage(device_, &image_info, nullptr, &source_image_), "vkCreateImage source");

		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device_, source_image_, &requirements);
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = requirements.size;
		alloc_info.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CheckVk(vkAllocateMemory(device_, &alloc_info, nullptr, &source_memory_), "vkAllocateMemory source");
		CheckVk(vkBindImageMemory(device_, source_image_, source_memory_, 0), "vkBindImageMemory source");

		VkCommandBuffer command_buffer = BeginOneTimeCommands();
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = source_image_;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &barrier);
		EndOneTimeCommands(command_buffer);
		source_view_ = CreatePlaneView(source_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void CreateOverlayImage() {
		const size_t byte_count = static_cast<size_t>(prepared_overlay_.width()) * prepared_overlay_.height() * 4;

		VkBuffer staging_buffer = VK_NULL_HANDLE;
		VkDeviceMemory staging_memory = VK_NULL_HANDLE;

		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = byte_count;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		CheckVk(vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer), "vkCreateBuffer staging");

		VkMemoryRequirements buffer_requirements{};
		vkGetBufferMemoryRequirements(device_, staging_buffer, &buffer_requirements);
		VkMemoryAllocateInfo buffer_alloc{};
		buffer_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		buffer_alloc.allocationSize = buffer_requirements.size;
		buffer_alloc.memoryTypeIndex = FindMemoryType(
			buffer_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		CheckVk(vkAllocateMemory(device_, &buffer_alloc, nullptr, &staging_memory), "vkAllocateMemory staging");
		CheckVk(vkBindBufferMemory(device_, staging_buffer, staging_memory, 0), "vkBindBufferMemory staging");

		void* mapped = nullptr;
		CheckVk(vkMapMemory(device_, staging_memory, 0, byte_count, 0, &mapped), "vkMapMemory staging");
		for (int y = 0; y < prepared_overlay_.height(); ++y) {
			std::memcpy(static_cast<char*>(mapped) + static_cast<size_t>(y) * prepared_overlay_.width() * 4,
						prepared_overlay_.constScanLine(y),
						static_cast<size_t>(prepared_overlay_.width()) * 4);
		}
		vkUnmapMemory(device_, staging_memory);

		VkImageCreateInfo image_info{};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_info.extent = {static_cast<uint32_t>(prepared_overlay_.width()), static_cast<uint32_t>(prepared_overlay_.height()), 1};
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		CheckVk(vkCreateImage(device_, &image_info, nullptr, &overlay_image_), "vkCreateImage overlay");

		VkMemoryRequirements image_requirements{};
		vkGetImageMemoryRequirements(device_, overlay_image_, &image_requirements);
		VkMemoryAllocateInfo image_alloc{};
		image_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		image_alloc.allocationSize = image_requirements.size;
		image_alloc.memoryTypeIndex = FindMemoryType(image_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CheckVk(vkAllocateMemory(device_, &image_alloc, nullptr, &overlay_memory_), "vkAllocateMemory overlay");
		CheckVk(vkBindImageMemory(device_, overlay_image_, overlay_memory_, 0), "vkBindImageMemory overlay");

		VkCommandBuffer command_buffer = BeginOneTimeCommands();

		VkImageMemoryBarrier to_transfer{};
		to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_transfer.image = overlay_image_;
		to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		to_transfer.subresourceRange.levelCount = 1;
		to_transfer.subresourceRange.layerCount = 1;
		to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &to_transfer);

		VkBufferImageCopy copy_region{};
		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageExtent = {static_cast<uint32_t>(prepared_overlay_.width()),
								   static_cast<uint32_t>(prepared_overlay_.height()),
								   1};
		vkCmdCopyBufferToImage(command_buffer, staging_buffer, overlay_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		VkImageMemoryBarrier to_shader{};
		to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_shader.image = overlay_image_;
		to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		to_shader.subresourceRange.levelCount = 1;
		to_shader.subresourceRange.layerCount = 1;
		to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &to_shader);

		VkImageMemoryBarrier output_barrier{};
		output_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		output_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		output_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		output_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		output_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		output_barrier.image = output_image_;
		output_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		output_barrier.subresourceRange.levelCount = 1;
		output_barrier.subresourceRange.layerCount = 1;
		output_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &output_barrier);

		EndOneTimeCommands(command_buffer);
		vkDestroyBuffer(device_, staging_buffer, nullptr);
		vkFreeMemory(device_, staging_memory, nullptr);
		overlay_view_ = CreatePlaneView(overlay_image_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void CreatePipeline() {
		const std::vector<uint32_t> code = LoadSpirvFile(VULKAN_DIRECT_PREVIEW_SHADER_PATH);
		VkShaderModuleCreateInfo shader_info{};
		shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shader_info.codeSize = code.size() * sizeof(uint32_t);
		shader_info.pCode = code.data();

		VkShaderModule shader_module = VK_NULL_HANDLE;
		CheckVk(vkCreateShaderModule(device_, &shader_info, nullptr, &shader_module), "vkCreateShaderModule");

		VkPipelineShaderStageCreateInfo stage_info{};
		stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage_info.module = shader_module;
		stage_info.pName = "main";

		VkComputePipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_info.stage = stage_info;
		pipeline_info.layout = pipeline_layout_;
		CheckVk(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_),
				"vkCreateComputePipelines");
		vkDestroyShaderModule(device_, shader_module, nullptr);
	}

	VkImageView CreatePlaneView(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask) const {
		VkImageViewCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = image;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = format;
		info.subresourceRange.aspectMask = aspect_mask;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;
		VkImageView view = VK_NULL_HANDLE;
		CheckVk(vkCreateImageView(device_, &info, nullptr, &view), "vkCreateImageView");
		return view;
	}

	void UpdateDescriptorSet(const std::array<VkImageView, 3>& video_views) {
		std::array<VkDescriptorImageInfo, 5> image_infos{};
		for (int i = 0; i < 3; ++i) {
			image_infos[i].sampler = sampler_;
			image_infos[i].imageView = video_views[i];
			image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		image_infos[3].sampler = sampler_;
		image_infos[3].imageView = overlay_view_;
		image_infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_infos[4].imageView = output_view_;
		image_infos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		std::array<VkWriteDescriptorSet, 5> writes{};
		for (uint32_t i = 0; i < 4; ++i) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = descriptor_set_;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &image_infos[i];
		}
		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = descriptor_set_;
		writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[4].pImageInfo = &image_infos[4];
		vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	void UploadSourceRgba(const AVFrame* frame) {
		const size_t byte_count = static_cast<size_t>(frame->width) * frame->height * 4;
		VkBuffer staging_buffer = VK_NULL_HANDLE;
		VkDeviceMemory staging_memory = VK_NULL_HANDLE;

		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = byte_count;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		CheckVk(vkCreateBuffer(device_, &buffer_info, nullptr, &staging_buffer), "vkCreateBuffer source staging");

		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(device_, staging_buffer, &requirements);
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = requirements.size;
		alloc_info.memoryTypeIndex = FindMemoryType(
			requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		CheckVk(vkAllocateMemory(device_, &alloc_info, nullptr, &staging_memory), "vkAllocateMemory source staging");
		CheckVk(vkBindBufferMemory(device_, staging_buffer, staging_memory, 0), "vkBindBufferMemory source staging");

		void* mapped = nullptr;
		CheckVk(vkMapMemory(device_, staging_memory, 0, byte_count, 0, &mapped), "vkMapMemory source staging");
		for (int y = 0; y < frame->height; ++y) {
			std::memcpy(static_cast<char*>(mapped) + static_cast<size_t>(y) * frame->width * 4,
						frame->data[0] + y * frame->linesize[0],
						static_cast<size_t>(frame->width) * 4);
		}
		vkUnmapMemory(device_, staging_memory);

		VkCommandBuffer command_buffer = BeginOneTimeCommands();
		VkImageMemoryBarrier to_transfer{};
		to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		to_transfer.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_transfer.image = source_image_;
		to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		to_transfer.subresourceRange.levelCount = 1;
		to_transfer.subresourceRange.layerCount = 1;
		to_transfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &to_transfer);

		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = {static_cast<uint32_t>(frame->width), static_cast<uint32_t>(frame->height), 1};
		vkCmdCopyBufferToImage(command_buffer, staging_buffer, source_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageMemoryBarrier to_shader{};
		to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_shader.image = source_image_;
		to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		to_shader.subresourceRange.levelCount = 1;
		to_shader.subresourceRange.layerCount = 1;
		to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &to_shader);
		EndOneTimeCommands(command_buffer);

		vkDestroyBuffer(device_, staging_buffer, nullptr);
		vkFreeMemory(device_, staging_memory, nullptr);
	}

	VkCommandBuffer BeginOneTimeCommands() {
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool_;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		VkCommandBuffer command_buffer = VK_NULL_HANDLE;
		CheckVk(vkAllocateCommandBuffers(device_, &alloc_info, &command_buffer), "vkAllocateCommandBuffers setup");

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		CheckVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer setup");
		return command_buffer;
	}

	void EndOneTimeCommands(VkCommandBuffer command_buffer) {
		CheckVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer setup");
		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
		VkFence fence = Submit(submit_info, "setup");
		WaitForFence(fence, "setup");
		vkDestroyFence(device_, fence, nullptr);
		vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
	}

	VkFence Submit(const VkSubmitInfo& submit_info, const std::string& context) {
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		VkFence fence = VK_NULL_HANDLE;
		CheckVk(vkCreateFence(device_, &fence_info, nullptr, &fence), "vkCreateFence " + context);
		bool queue_locked = false;

		try {
			// Hold FFmpeg's Vulkan queue lock only around queue submission.
			// Keeping the lock during vkWaitForFences can deadlock with decoder threads
			// that need to submit work which advances our waited timeline semaphores.
			vulkan_device_context_->lock_queue(device_context_, static_cast<uint32_t>(compute_queue_family_), 0);
			queue_locked = true;
			CheckVk(vkQueueSubmit(compute_queue_, 1, &submit_info, fence), "vkQueueSubmit " + context);
			vulkan_device_context_->unlock_queue(device_context_, static_cast<uint32_t>(compute_queue_family_), 0);
			queue_locked = false;
		} catch (...) {
			// Best-effort unlock in case an exception occurred before normal unlock.
			if (queue_locked)
				vulkan_device_context_->unlock_queue(device_context_, static_cast<uint32_t>(compute_queue_family_), 0);
			vkDestroyFence(device_, fence, nullptr);
			throw;
		}
		return fence;
	}

	void WaitForFence(VkFence fence, const std::string& context) {
		if (fence == VK_NULL_HANDLE)
			return;
		const VkResult wait_result = vkWaitForFences(device_, 1, &fence, VK_TRUE, kQueueWaitTimeoutNs);
		if (wait_result == VK_TIMEOUT) {
			throw std::runtime_error("Vulkan queue wait timeout in " + context +
									 "; possible GPU sync deadlock in direct Vulkan path");
		}
		CheckVk(wait_result, "vkWaitForFences " + context);
	}

	const BenchmarkOptions& options_;
	AVHWFramesContext* frames_context_ = nullptr;
	AVVulkanFramesContext* vulkan_frames_context_ = nullptr;
	AVHWDeviceContext* device_context_ = nullptr;
	AVVulkanDeviceContext* vulkan_device_context_ = nullptr;
	VkDevice device_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	int compute_queue_family_ = -1;
	int chroma_mode_ = 0;
	bool software_input_ = false;
	VkQueue compute_queue_ = VK_NULL_HANDLE;
	RenderLayout layout_{};
	QImage prepared_overlay_;
	int output_width_ = 0;
	int output_height_ = 0;
	VkCommandPool command_pool_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
	VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
	VkPipeline pipeline_ = VK_NULL_HANDLE;
	VkSampler sampler_ = VK_NULL_HANDLE;
	VkImage overlay_image_ = VK_NULL_HANDLE;
	VkDeviceMemory overlay_memory_ = VK_NULL_HANDLE;
	VkImageView overlay_view_ = VK_NULL_HANDLE;
	VkImage output_image_ = VK_NULL_HANDLE;
	VkDeviceMemory output_memory_ = VK_NULL_HANDLE;
	VkImageView output_view_ = VK_NULL_HANDLE;
	VkImage source_image_ = VK_NULL_HANDLE;
	VkDeviceMemory source_memory_ = VK_NULL_HANDLE;
	VkImageView source_view_ = VK_NULL_HANDLE;
	VkFence pending_fence_ = VK_NULL_HANDLE;
	VkCommandBuffer pending_command_buffer_ = VK_NULL_HANDLE;
	std::array<VkImageView, 3> pending_video_views_{};
	bool pending_submission_ = false;
};

static TimingStats RunVulkanUploadBenchmark(const BenchmarkOptions& options, const QImage& overlay_image,
											DecodeKind decode_kind, bool& used_hw_decode) {
	TimingStats stats;
	const auto total_start = Clock::now();

	GenericDecodeReader decoder(options, decode_kind);
	decoder.Open();

	ScopedFrame first_frame;
	double decode_ms = 0.0;
	if (!decoder.NextFrame(first_frame, decode_ms)) {
		decoder.Close();
		used_hw_decode = false;
		return stats;
	}

	used_hw_decode = decoder.UsingHwDecode();
	stats.decode_ms += decode_ms;

	AVBufferRef* vulkan_device_context = nullptr;
	try {
		const RenderLayout layout = ComputeRenderLayout(options, first_frame.get()->width, first_frame.get()->height);
		RgbaFrameConverter converter;
		RgbaFrameResizer resizer;

		CheckAv(av_hwdevice_ctx_create(&vulkan_device_context, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0),
				"av_hwdevice_ctx_create vulkan upload device");
		DirectVulkanPreviewCompositor compositor(
			options,
			overlay_image,
			vulkan_device_context,
			layout.output_width,
			layout.output_height);

		const int frame_limit = options.max_frames;
		ScopedFrame current_frame = std::move(first_frame);
		int frame_index = 0;
		while (current_frame && frame_index < frame_limit) {
			const auto upload_start = Clock::now();
			ScopedFrame rgba_input_frame = converter.Convert(current_frame.get());
			ScopedFrame scaled_input_frame = resizer.Resize(rgba_input_frame.get(), layout.output_width, layout.output_height);
			ForceOpaqueAlpha(scaled_input_frame.get());
			ScopedFrame main_input_frame(av_frame_clone(scaled_input_frame.get()));
			const auto upload_end = Clock::now();
			stats.upload_ms += std::chrono::duration<double, std::milli>(upload_end - upload_start).count();

			if (!main_input_frame)
				throw std::runtime_error("Unable to clone RGBA main frame");

			const auto composite_start = Clock::now();
			compositor.CompositeSoftwareRgba(main_input_frame.get());
			if (ShouldDumpFrame(options, frame_index + 1)) {
				SaveDumpImage(
					options,
					decode_kind == DecodeKind::Cuda ? "CUDA->Vk" :
					decode_kind == DecodeKind::Vaapi ? "VAAPI->Vk" :
					decode_kind == DecodeKind::Vulkan ? "Vulkan->Vk" :
					"CPU->Vk",
					frame_index + 1,
					compositor.ReadbackOutput());
			}
			const auto composite_end = Clock::now();
			stats.composite_ms += std::chrono::duration<double, std::milli>(composite_end - composite_start).count();
			stats.frames++;
			frame_index++;

			if (frame_index >= frame_limit)
				break;

			current_frame = ScopedFrame();
			double next_decode_ms = 0.0;
			if (!decoder.NextFrame(current_frame, next_decode_ms))
				break;
			stats.decode_ms += next_decode_ms;
			used_hw_decode = used_hw_decode || decoder.UsingHwDecode();
		}
	} catch (...) {
		if (vulkan_device_context)
			av_buffer_unref(&vulkan_device_context);
		decoder.Close();
		throw;
	}

	if (vulkan_device_context)
		av_buffer_unref(&vulkan_device_context);

	decoder.Close();
	stats.total_ms = std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
	return stats;
}

static TimingStats RunVulkanDirectBenchmark(const BenchmarkOptions& options, const QImage& overlay_image,
											bool& used_hw_decode) {
	TimingStats stats;
	const auto total_start = Clock::now();

	GenericDecodeReader decoder(options, DecodeKind::Vulkan);
	decoder.Open();

	ScopedFrame first_frame;
	double decode_ms = 0.0;
	if (!decoder.NextFrame(first_frame, decode_ms)) {
		decoder.Close();
		used_hw_decode = false;
		return stats;
	}

	used_hw_decode = decoder.UsingHwDecode();
	if (!used_hw_decode || first_frame.get()->format != AV_PIX_FMT_VULKAN || !first_frame.get()->hw_frames_ctx) {
		decoder.Close();
		throw std::runtime_error("direct Vulkan row requires real Vulkan hw decode; try RADV_PERFTEST=video_decode");
	}

	stats.decode_ms += decode_ms;

	try {
		DirectVulkanPreviewCompositor compositor(options, overlay_image, first_frame.get()->hw_frames_ctx);

		const int frame_limit = options.max_frames;
		ScopedFrame current_frame = std::move(first_frame);
		int frame_index = 0;
		while (current_frame && frame_index < frame_limit) {
			if (current_frame.get()->format != AV_PIX_FMT_VULKAN)
				throw std::runtime_error("decode fell back away from Vulkan during direct row");

			ScopedFrame main_input_frame(av_frame_clone(current_frame.get()));
			if (!main_input_frame)
				throw std::runtime_error("Unable to clone direct Vulkan frame");

			const auto submit_start = Clock::now();
			compositor.Submit(main_input_frame.get());
			const auto submit_end = Clock::now();
			stats.composite_ms += std::chrono::duration<double, std::milli>(submit_end - submit_start).count();

			ScopedFrame next_frame;
			double next_decode_ms = 0.0;
			if (frame_index + 1 < frame_limit && decoder.NextFrame(next_frame, next_decode_ms)) {
				stats.decode_ms += next_decode_ms;
				used_hw_decode = used_hw_decode || decoder.UsingHwDecode();
			}

			const auto wait_start = Clock::now();
			compositor.WaitForPendingSubmission("direct compositor");
			const auto wait_end = Clock::now();
			stats.composite_ms += std::chrono::duration<double, std::milli>(wait_end - wait_start).count();
			if (ShouldDumpFrame(options, frame_index + 1)) {
				SaveDumpImage(options, "VulkanDirect->Vk", frame_index + 1, compositor.ReadbackOutput());
			}
			stats.frames++;
			frame_index++;
			current_frame = std::move(next_frame);
		}
	} catch (...) {
		decoder.Close();
		throw;
	}

	decoder.Close();
	stats.total_ms = std::chrono::duration<double, std::milli>(Clock::now() - total_start).count();
	return stats;
}

static BenchmarkResult MakeCpuBaselineResult(const BenchmarkOptions& options, const QImage& overlay_image) {
	BenchmarkResult result;
	result.name = "CPU->CPU";
	result.decode_backend = "FFmpegReader CPU";
	result.composite_backend = "QPainter CPU";
	result.stats = RunCpuBenchmark(options, overlay_image);
	return result;
}

static BenchmarkResult MakeCpuCompositeResult(const BenchmarkOptions& options, const QImage& overlay_image,
											  const std::string& name, DecodeKind decode_kind,
											  const std::string& decode_label) {
	BenchmarkResult result;
	result.name = name;
	result.decode_backend = decode_label;
	result.composite_backend = "QPainter CPU";
	result.hw_decode_requested = decode_kind != DecodeKind::Software;
	result.readback_to_cpu = decode_kind != DecodeKind::Software;
	result.note = decode_kind == DecodeKind::Cuda
		? "CUDA decode -> CPU readback -> QPainter"
		: decode_kind == DecodeKind::Vaapi
		? "HW decode -> CPU readback -> QPainter"
		: decode_kind == DecodeKind::Vulkan
			? "HW decode if available -> CPU readback -> QPainter"
			: "Software decode -> QPainter";
	result.stats = RunCpuCompositeBenchmark(options, overlay_image, decode_kind, result.hw_decode_used);
	return result;
}

static BenchmarkResult MakeVulkanCompositeResult(const BenchmarkOptions& options, const QImage& overlay_image,
												 const std::string& name, DecodeKind decode_kind,
												 const std::string& decode_label) {
	BenchmarkResult result;
	result.name = name;
	result.decode_backend = decode_label;
	result.composite_backend = "custom_vulkan";
	result.hw_decode_requested = decode_kind != DecodeKind::Software;
	result.readback_to_cpu = decode_kind == DecodeKind::Cuda || decode_kind == DecodeKind::Vaapi || decode_kind == DecodeKind::Vulkan;
	result.upload_to_vulkan = true;
	result.note = decode_kind == DecodeKind::Software
		? "Software decode -> RGBA upload -> custom Vulkan scale + alpha overlay"
		: decode_kind == DecodeKind::Cuda
			? "CUDA decode -> CPU readback -> custom Vulkan scale + alpha overlay"
		: decode_kind == DecodeKind::Vaapi
			? "VAAPI decode -> CPU readback -> custom Vulkan scale + alpha overlay"
			: "Vulkan decode -> CPU readback -> custom Vulkan scale + alpha overlay";
	result.stats = RunVulkanUploadBenchmark(options, overlay_image, decode_kind, result.hw_decode_used);
	return result;
}

static BenchmarkResult MakeVulkanDirectResult(const BenchmarkOptions& options, const QImage& overlay_image) {
	BenchmarkResult result;
	result.name = "VulkanDirect->Vk";
	result.decode_backend = "Vulkan";
	result.composite_backend = "custom_vulkan";
	result.hw_decode_requested = true;
	result.hw_decode_used = false;
	result.readback_to_cpu = false;
	result.upload_to_vulkan = false;
	result.note = "Vulkan decode frame stays on Vulkan; custom GPU scale + alpha overlay";
	result.stats = RunVulkanDirectBenchmark(options, overlay_image, result.hw_decode_used);
	return result;
}

static std::string FormatDouble(double value) {
	std::ostringstream out;
	out << std::fixed << std::setprecision(3) << value;
	return out.str();
}

static std::string FormatBool(bool value) {
	return value ? "yes" : "no";
}

static std::string PadCell(const std::string& value, size_t width) {
	if (value.size() >= width)
		return value.substr(0, width);
	return value + std::string(width - value.size(), ' ');
}

static void PrintResultTable(const std::vector<BenchmarkResult>& results) {
	struct Column { std::string title; size_t width; };
	const std::vector<Column> columns = {
		{"Path", 16}, {"Decode", 18}, {"Composite", 15}, {"HW", 4},
		{"Readback", 8}, {"Upload", 6}, {"Frames", 6},
		{"DecAvg", 8}, {"UpAvg", 8}, {"CompAvg", 8}, {"FPS", 8}
	};

	auto print_separator = [&]() {
		std::cout << "+";
		for (const auto& column : columns) {
			std::cout << std::string(column.width + 2, '-') << "+";
		}
		std::cout << "\n";
	};

	print_separator();
	std::cout << "|";
	for (const auto& column : columns) {
		std::cout << " " << PadCell(column.title, column.width) << " |";
	}
	std::cout << "\n";
	print_separator();

	for (const auto& result : results) {
		std::vector<std::string> cells = {
			result.name,
			result.decode_backend,
			result.composite_backend,
			FormatBool(result.hw_decode_used),
			FormatBool(result.readback_to_cpu),
			FormatBool(result.upload_to_vulkan),
			std::to_string(result.stats.frames),
			FormatDouble(result.stats.frames ? result.stats.decode_ms / result.stats.frames : 0.0),
			FormatDouble(result.stats.frames ? result.stats.upload_ms / result.stats.frames : 0.0),
			FormatDouble(result.stats.frames ? result.stats.composite_ms / result.stats.frames : 0.0),
			FormatDouble(result.stats.total_ms > 0.0 ? (result.stats.frames * 1000.0) / result.stats.total_ms : 0.0)
		};

		std::cout << "|";
		for (size_t i = 0; i < columns.size(); ++i) {
			std::cout << " " << PadCell(cells[i], columns[i].width) << " |";
		}
		std::cout << "\n";
		if (!result.note.empty()) {
			std::cout << "| " << PadCell("note: " + result.note, 121) << " |\n";
		}
		print_separator();
	}
}

static BenchmarkOptions ParseOptions(int argc, char* argv[]) {
	BenchmarkOptions options;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if ((arg == "--video" || arg == "-v") && i + 1 < argc) {
			options.video_path = argv[++i];
		} else if ((arg == "--overlay" || arg == "-o") && i + 1 < argc) {
			options.overlay_path = argv[++i];
		} else if (arg == "--mode" && i + 1 < argc) {
			options.mode = argv[++i];
			if (options.mode != "preview" && options.mode != "export")
				throw std::runtime_error("Unknown mode: " + options.mode);
		} else if (arg == "--dump-dir" && i + 1 < argc) {
			options.dump_dir = argv[++i];
		} else if (arg == "--dump-every" && i + 1 < argc) {
			options.dump_every = std::max(0, std::atoi(argv[++i]));
		} else if ((arg == "--frames" || arg == "-n") && i + 1 < argc) {
			options.max_frames = std::max(1, std::atoi(argv[++i]));
		} else if (arg == "--row" && i + 1 < argc) {
			options.row_name = argv[++i];
		} else if (arg == "--vaapi-device" && i + 1 < argc) {
			options.vaapi_device = argv[++i];
		} else if (arg == "--cpu-only") {
			options.run_vulkan = false;
		} else if (arg == "--vulkan-only") {
			options.run_cpu = false;
		} else if (arg == "--help" || arg == "-h") {
			std::cout
				<< "Usage: openshot-vulkan-benchmark [--video <path>] [--overlay <path>] [--frames <n>]\n"
				<< "                                  [--mode <preview|export>] [--vaapi-device <path>] [--row <name>]\n"
				<< "                                  [--dump-dir <dir>] [--dump-every <n>]\n"
				<< "                                  [--cpu-only | --vulkan-only] (select compositor)\n"
				<< "Rows: CPU->CPU, CPU->Vk, VAAPI->CPU, VAAPI->Vk, CUDA->CPU, CUDA->Vk, Vulkan->CPU, VulkanDirect->Vk\n";
			std::exit(0);
		} else {
			throw std::runtime_error("Unknown argument: " + arg);
		}
	}
	return options;
}

} // namespace

int main(int argc, char* argv[]) {
	try {
		const BenchmarkOptions options = ParseOptions(argc, argv);

		std::cout << "OpenShot Vulkan Benchmark\n";
		std::cout << "video=" << options.video_path << "\n";
		std::cout << "overlay=" << options.overlay_path << "\n";
		std::cout << "vaapi_device=" << options.vaapi_device << "\n";
		std::cout << "frames=" << options.max_frames << "\n";
		std::cout << "mode=" << options.mode << "\n";
		if (!options.dump_dir.empty())
			std::cout << "dump_dir=" << options.dump_dir << " dump_every=" << options.dump_every << "\n";
		if (!options.row_name.empty())
			std::cout << "row=" << options.row_name << "\n";
		if (options.output_width > 0 && options.output_height > 0) {
			std::cout << "output=" << options.output_width << "x" << options.output_height << "\n";
		} else if (options.mode == "preview") {
			std::cout << "output=fit-" << options.preview_width << "x" << options.preview_height << "\n";
		} else {
			std::cout << "output=input-size\n";
		}

		const VulkanSummary summary = QueryVulkanSummary();
		PrintVulkanSummary(summary);
		const BackendAvailability backends = ProbeBackends(options);
		std::cout << "backend_probe:"
				  << " vaapi=" << (backends.vaapi ? "yes" : "no")
				  << " cuda=" << (backends.cuda ? "yes" : "no")
				  << " vulkan_runtime=" << (backends.vulkan_runtime ? "yes" : "no")
				  << "\n";

		const QImage overlay_image = LoadOverlayImage(options);
		std::vector<BenchmarkResult> results;
		auto row_enabled = [&](const std::string& name) {
			const bool vulkan_composite = name.size() >= 4 && name.compare(name.size() - 4, 4, "->Vk") == 0;
			return (vulkan_composite ? options.run_vulkan : options.run_cpu) &&
				(options.row_name.empty() || options.row_name == name);
		};
		auto run_row = [&](BenchmarkResult (*fn)(const BenchmarkOptions&, const QImage&),
						   const std::string& fallback_name) {
			if (!row_enabled(fallback_name))
				return;
			try {
				results.push_back(fn(options, overlay_image));
			} catch (const std::exception& exc) {
				BenchmarkResult failed;
				failed.name = fallback_name;
				failed.decode_backend = "n/a";
				failed.composite_backend = "n/a";
				failed.note = std::string("skipped: ") + exc.what();
				failed.skipped = true;
				results.push_back(failed);
			}
		};
		auto run_custom_row = [&](auto builder, const std::string& fallback_name) {
			if (!row_enabled(fallback_name))
				return;
			try {
				results.push_back(builder());
			} catch (const std::exception& exc) {
				BenchmarkResult failed;
				failed.name = fallback_name;
				failed.decode_backend = "n/a";
				failed.composite_backend = "n/a";
				failed.note = std::string("skipped: ") + exc.what();
				failed.skipped = true;
				results.push_back(failed);
			}
		};
		auto add_unavailable_row = [&](const std::string& name, const std::string& reason) {
			if (!row_enabled(name))
				return;
			BenchmarkResult unavailable;
			unavailable.name = name;
			unavailable.decode_backend = "n/a";
			unavailable.composite_backend = "n/a";
			unavailable.note = "unavailable: " + reason;
			unavailable.skipped = true;
			results.push_back(unavailable);
		};

		run_row(&MakeCpuBaselineResult, "CPU->CPU");
		if (backends.vulkan_runtime)
			run_custom_row([&]() { return MakeVulkanCompositeResult(options, overlay_image, "CPU->Vk", DecodeKind::Software, "libavcodec SW"); }, "CPU->Vk");
		else
			add_unavailable_row("CPU->Vk", "Vulkan runtime unavailable");

		if (backends.vaapi) {
			run_custom_row([&]() { return MakeCpuCompositeResult(options, overlay_image, "VAAPI->CPU", DecodeKind::Vaapi, "VAAPI"); }, "VAAPI->CPU");
			if (backends.vulkan_runtime)
				run_custom_row([&]() { return MakeVulkanCompositeResult(options, overlay_image, "VAAPI->Vk", DecodeKind::Vaapi, "VAAPI"); }, "VAAPI->Vk");
			else
				add_unavailable_row("VAAPI->Vk", "Vulkan runtime unavailable");
		} else {
			add_unavailable_row("VAAPI->CPU", "VAAPI unavailable");
			add_unavailable_row("VAAPI->Vk", "VAAPI unavailable");
		}

		if (backends.cuda) {
			run_custom_row([&]() { return MakeCpuCompositeResult(options, overlay_image, "CUDA->CPU", DecodeKind::Cuda, "CUDA"); }, "CUDA->CPU");
			if (backends.vulkan_runtime)
				run_custom_row([&]() { return MakeVulkanCompositeResult(options, overlay_image, "CUDA->Vk", DecodeKind::Cuda, "CUDA"); }, "CUDA->Vk");
			else
				add_unavailable_row("CUDA->Vk", "Vulkan runtime unavailable");
		} else {
			add_unavailable_row("CUDA->CPU", "CUDA unavailable");
			add_unavailable_row("CUDA->Vk", "CUDA unavailable");
		}

		if (backends.vulkan_runtime) {
			run_custom_row([&]() { return MakeCpuCompositeResult(options, overlay_image, "Vulkan->CPU", DecodeKind::Vulkan, "Vulkan"); }, "Vulkan->CPU");
			run_custom_row([&]() { return MakeVulkanDirectResult(options, overlay_image); }, "VulkanDirect->Vk");
		} else {
			add_unavailable_row("Vulkan->CPU", "Vulkan runtime unavailable");
			add_unavailable_row("VulkanDirect->Vk", "Vulkan runtime unavailable");
		}
		if (results.empty())
			throw std::runtime_error("No rows selected: check --row and compositor filters");

		for (const auto& result : results) {
			result.stats.Print(result.name + " / " + result.decode_backend + " / " + result.composite_backend);
			if (!result.note.empty())
				std::cout << "note=" << result.note << "\n";
			if (result.hw_decode_requested)
				std::cout << "hw_decode_used=" << (result.hw_decode_used ? "true" : "false") << "\n";
		}

		std::cout << "\nMatrix Summary\n";
		PrintResultTable(results);

		const bool processed_frames = std::any_of(results.begin(), results.end(),
			[](const BenchmarkResult& result) { return !result.skipped && result.stats.frames > 0; });
		if (!processed_frames)
			throw std::runtime_error("No benchmark row successfully processed frames");
		return 0;
	} catch (const std::exception& exc) {
		std::cerr << "Benchmark failed: " << exc.what() << "\n";
		return 1;
	}
}
