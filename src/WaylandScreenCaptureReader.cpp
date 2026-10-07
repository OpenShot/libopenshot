/**
 * @file
 * @brief Wayland screen capture backend using xdg-desktop-portal and PipeWire
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ScreenCaptureReader.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <pipewire/pipewire.h>
#include <spa/buffer/buffer.h>
#include <spa/buffer/meta.h>
#include <spa/param/buffers.h>
#include <spa/pod/builder.h>
#include <spa/param/video/format-utils.h>
#include <unistd.h>

#include "Exceptions.h"
#include "Frame.h"
#include "WaylandBufferUtilities.h"
#include "Logger.h"

using namespace openshot;

namespace
{
	const char* PORTAL_BUS = "org.freedesktop.portal.Desktop";
	const char* PORTAL_PATH = "/org/freedesktop/portal/desktop";
	const char* SCREENCAST_IFACE = "org.freedesktop.portal.ScreenCast";

	std::string gerror_message(GError* error)
	{
		if (!error) {
			return "unknown error";
		}
		std::string message = error->message ? error->message : "unknown error";
		g_error_free(error);
		return message;
	}

	std::string token_for(const char* prefix)
	{
		static uint64_t counter = 0;
		std::stringstream token;
		token << prefix << "_" << ++counter;
		return token.str();
	}

	struct PortalResponse
	{
		bool done = false;
		bool timed_out = false;
		uint32_t code = 1;
		GVariant* results = nullptr;
		GMainLoop* loop = nullptr;

		~PortalResponse()
		{
			if (results) {
				g_variant_unref(results);
			}
		}
	};

	void portal_response_callback(
		GDBusConnection*,
		const gchar*,
		const gchar*,
		const gchar*,
		const gchar*,
		GVariant* parameters,
		gpointer user_data)
	{
		auto* response = static_cast<PortalResponse*>(user_data);
		GVariant* results = nullptr;
		g_variant_get(parameters, "(u@a{sv})", &response->code, &results);
		response->results = results;
		response->done = true;
		if (response->loop) {
			g_main_loop_quit(response->loop);
		}
	}

	gboolean portal_response_timeout(gpointer user_data)
	{
		auto* response = static_cast<PortalResponse*>(user_data);
		response->timed_out = true;
		if (response->loop) {
			g_main_loop_quit(response->loop);
		}
		return G_SOURCE_REMOVE;
	}

	GVariant* wait_for_portal_response(GDBusConnection* connection, const std::string& handle)
	{
		PortalResponse response;
		response.loop = g_main_loop_new(nullptr, FALSE);
		const guint timeout = g_timeout_add_seconds(60, portal_response_timeout, &response);
		const guint subscription = g_dbus_connection_signal_subscribe(
			connection,
			PORTAL_BUS,
			"org.freedesktop.portal.Request",
			"Response",
			handle.c_str(),
			nullptr,
			G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
			portal_response_callback,
			&response,
			nullptr);

		g_main_loop_run(response.loop);
		g_dbus_connection_signal_unsubscribe(connection, subscription);
		if (!response.timed_out) {
			g_source_remove(timeout);
		}
		g_main_loop_unref(response.loop);
		response.loop = nullptr;

		if (response.timed_out) {
			throw InvalidOptions("Timed out waiting for Wayland screen capture permission.");
		}
		if (!response.done || response.code != 0) {
			throw InvalidOptions("Wayland screen capture permission was denied or cancelled.");
		}
		GVariant* results = response.results;
		response.results = nullptr;
		return results;
	}

	std::string call_portal_request(
		GDBusConnection* connection,
		const char* method,
		GVariant* parameters)
	{
		GError* error = nullptr;
		GVariant* result = g_dbus_connection_call_sync(
			connection,
			PORTAL_BUS,
			PORTAL_PATH,
			SCREENCAST_IFACE,
			method,
			parameters,
			G_VARIANT_TYPE("(o)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			nullptr,
			&error);
		if (!result) {
			throw InvalidOptions("Wayland portal " + std::string(method) + " failed: " + gerror_message(error));
		}

		const char* handle = nullptr;
		g_variant_get(result, "(&o)", &handle);
		std::string handle_path = handle ? handle : "";
		g_variant_unref(result);
		return handle_path;
	}

	struct PortalStreamInfo
	{
		uint32_t node_id = 0;
		uint64_t pipewire_serial = 0;
		uint32_t source_type = 0;
		int width = 0;
		int height = 0;
	};

	PortalStreamInfo stream_info_from_results(GVariant* results)
	{
		GVariant* streams = g_variant_lookup_value(results, "streams", G_VARIANT_TYPE("a(ua{sv})"));
		if (!streams || g_variant_n_children(streams) == 0) {
			if (streams) {
				g_variant_unref(streams);
			}
			throw InvalidOptions("Wayland portal did not return a PipeWire stream.");
		}

		PortalStreamInfo stream_info;
		GVariant* properties = nullptr;
		GVariant* child = g_variant_get_child_value(streams, 0);
		g_variant_get(child, "(u@a{sv})", &stream_info.node_id, &properties);
		if (properties) {
			uint64_t serial = 0;
			uint32_t source_type = 0;
			int width = 0;
			int height = 0;
			if (g_variant_lookup(properties, "pipewire-serial", "t", &serial)) {
				stream_info.pipewire_serial = serial;
			}
			if (g_variant_lookup(properties, "source_type", "u", &source_type)) {
				stream_info.source_type = source_type;
			}
			if (g_variant_lookup(properties, "size", "(ii)", &width, &height)) {
				stream_info.width = width;
				stream_info.height = height;
			}
			g_variant_unref(properties);
		}
		g_variant_unref(child);
		g_variant_unref(streams);
		return stream_info;
	}

	int open_pipewire_remote(GDBusConnection* connection, const std::string& session_handle)
	{
		GVariantBuilder options;
		g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);

		GError* error = nullptr;
		GUnixFDList* out_fds = nullptr;
		GVariant* result = g_dbus_connection_call_with_unix_fd_list_sync(
			connection,
			PORTAL_BUS,
			PORTAL_PATH,
			SCREENCAST_IFACE,
			"OpenPipeWireRemote",
			g_variant_new("(oa{sv})", session_handle.c_str(), &options),
			G_VARIANT_TYPE("(h)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			nullptr,
			&out_fds,
			nullptr,
			&error);
		if (!result) {
			throw InvalidOptions("Wayland portal OpenPipeWireRemote failed: " + gerror_message(error));
		}

		int fd_index = -1;
		g_variant_get(result, "(h)", &fd_index);
		g_variant_unref(result);
		int fd = g_unix_fd_list_get(out_fds, fd_index, &error);
		g_object_unref(out_fds);
		if (fd < 0) {
			throw InvalidOptions("Unable to obtain PipeWire remote file descriptor: " + gerror_message(error));
		}
		return fd;
	}

	struct CapturedFrame
	{
		int width = 0;
		int height = 0;
		std::vector<unsigned char> rgba;
	};
}

class WaylandScreenCaptureReader final : public ScreenCaptureReader::CaptureBackendReader
{
public:
	WaylandScreenCaptureReader(const ScreenCaptureSettings& new_settings, ReaderInfo& new_info)
		: settings(new_settings)
		, info(new_info)
		, connection(nullptr)
		, thread_loop(nullptr)
		, context(nullptr)
		, core(nullptr)
		, stream(nullptr)
		, session_closed_subscription(0)
		, frames_read(0)
		, dropped_packets(0)
		, open(false)
		, streaming(false)
		, stream_error(false)
		, crop_logged(false)
		, have_last_header_sequence(false)
		, last_header_sequence(0)
		, header_sequence_ordering_active(false)
		, have_last_header_pts(false)
		, last_header_pts(0)
		, header_drop_log_count(0)
	{
	}

	~WaylandScreenCaptureReader() override
	{
		Close();
	}

	void Open() override
	{
		if (open) {
			return;
		}

		GError* error = nullptr;
		connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
		if (!connection) {
			throw InvalidOptions("Unable to connect to the session bus for Wayland capture: " + gerror_message(error));
		}

		session_handle = CreatePortalSession();
		SubscribePortalSessionClosed();
		SelectPortalSources(session_handle);
		stream_info = StartPortalSession(session_handle);
		ApplyPortalStreamInfo();
		Logger::Instance()->Log(
			"Wayland portal stream selected: node_id=" + std::to_string(stream_info.node_id) +
			" pipewire_serial=" + std::to_string(stream_info.pipewire_serial) +
			" source_type=" + std::to_string(stream_info.source_type) +
			" portal_size=" + std::to_string(stream_info.width) + "x" + std::to_string(stream_info.height));
		const int pipewire_fd = open_pipewire_remote(connection, session_handle);
		OpenPipeWireStream(pipewire_fd);
		open = true;
	}

	void Close() override
	{
		open = false;
		streaming = false;

		if (thread_loop) {
			pw_thread_loop_stop(thread_loop);
		}
		if (connection && session_closed_subscription) {
			g_dbus_connection_signal_unsubscribe(connection, session_closed_subscription);
			session_closed_subscription = 0;
		}
		if (connection && !session_handle.empty()) {
			GError* error = nullptr;
			GVariant* result = g_dbus_connection_call_sync(
				connection,
				PORTAL_BUS,
				session_handle.c_str(),
				"org.freedesktop.portal.Session",
				"Close",
				nullptr,
				nullptr,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				nullptr,
				&error);
			if (result) {
				g_variant_unref(result);
			} else if (error) {
				g_error_free(error);
			}
			session_handle.clear();
		}
		if (stream) {
			pw_stream_destroy(stream);
			stream = nullptr;
		}
		if (core) {
			pw_core_disconnect(core);
			core = nullptr;
		}
		if (context) {
			pw_context_destroy(context);
			context = nullptr;
		}
		if (thread_loop) {
			pw_thread_loop_destroy(thread_loop);
			thread_loop = nullptr;
		}
		if (connection) {
			g_object_unref(connection);
			connection = nullptr;
		}
	}

	bool IsOpen() const override
	{
		return open;
	}

	CaptureReaderStats GetStats() const override
	{
		CaptureReaderStats stats;
		stats.is_open = open;
		stats.frames_read = frames_read;
		stats.dropped_packets = dropped_packets;
		const double fps = settings.fps.den != 0 ? static_cast<double>(settings.fps.num) / static_cast<double>(settings.fps.den) : 0.0;
		stats.duration = fps > 0.0 ? static_cast<double>(frames_read) / fps : 0.0;
		return stats;
	}

	std::shared_ptr<Frame> GetFrame(int64_t number) override
	{
		CapturedFrame captured;
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			const auto wait_duration = std::chrono::milliseconds(
				wayland::DamageFrameWaitMilliseconds(
					settings.fps.num,
					settings.fps.den,
					have_last_frame));
			const auto deadline = std::chrono::steady_clock::now() + wait_duration;
			while (frame_queue.empty() && !stream_error && open && std::chrono::steady_clock::now() < deadline) {
				lock.unlock();
				while (g_main_context_iteration(nullptr, FALSE)) {
				}
				lock.lock();
				queue_condition.wait_for(lock, std::chrono::milliseconds(100));
			}
			if (frame_queue.empty() && !stream_error && open) {
				if (have_last_frame) {
					captured = last_frame;
				} else {
					throw InvalidFile("Timed out waiting for the first Wayland capture frame.", "wayland");
				}
			}
			if (stream_error) {
				throw InvalidFile("Wayland capture stream failed.", "wayland");
			}
			if (frame_queue.empty()) {
				if (!open) {
					throw ReaderClosed("The Wayland screen capture stream is closed.");
				}
				if (!have_last_frame) {
					throw InvalidFile("Timed out waiting for the first Wayland capture frame.", "wayland");
				}
			} else {
				captured = std::move(frame_queue.front());
				frame_queue.pop_front();
				last_frame = captured;
				have_last_frame = true;
			}
		}

		const int bytes_per_pixel = 4;
		const size_t buffer_size = static_cast<size_t>(captured.width) * captured.height * bytes_per_pixel;
		unsigned char* buffer = static_cast<unsigned char*>(malloc(buffer_size));
		if (!buffer) {
			throw OutOfMemory("Unable to allocate Wayland capture frame buffer.", "wayland");
		}
		std::memcpy(buffer, captured.rgba.data(), buffer_size);

		auto frame = std::make_shared<Frame>(number, captured.width, captured.height, "#000000");
		frame->AddImage(captured.width, captured.height, bytes_per_pixel, QImage::Format_RGBA8888, buffer);
		frames_read++;
		return frame;
	}

private:
	std::string CreatePortalSession()
	{
		GVariantBuilder options;
		g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token_for("openshot_create").c_str()));
		g_variant_builder_add(&options, "{sv}", "session_handle_token", g_variant_new_string(token_for("openshot_session").c_str()));

		const std::string handle = call_portal_request(connection, "CreateSession", g_variant_new("(a{sv})", &options));
		GVariant* results = wait_for_portal_response(connection, handle);

		const char* session = nullptr;
		if (!g_variant_lookup(results, "session_handle", "&s", &session) || !session) {
			g_variant_unref(results);
			throw InvalidOptions("Wayland portal did not return a screencast session handle.");
		}
		std::string session_handle = session;
		g_variant_unref(results);
		return session_handle;
	}

	void SubscribePortalSessionClosed()
	{
		if (!connection || session_handle.empty()) {
			return;
		}
		session_closed_subscription = g_dbus_connection_signal_subscribe(
			connection,
			PORTAL_BUS,
			"org.freedesktop.portal.Session",
			"Closed",
			session_handle.c_str(),
			nullptr,
			G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
			OnPortalSessionClosed,
			this,
			nullptr);
	}

	void SelectPortalSources(const std::string& session_handle)
	{
		const uint32_t source_monitor = 1;
		const uint32_t source_window = 2;
		const uint32_t cursor_hidden = 1;
		const uint32_t cursor_embedded = 2;

		GVariantBuilder options;
		g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token_for("openshot_select").c_str()));
		g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(source_monitor | source_window));
		g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(FALSE));
		g_variant_builder_add(&options, "{sv}", "cursor_mode", g_variant_new_uint32(settings.include_cursor ? cursor_embedded : cursor_hidden));

		const std::string handle = call_portal_request(
			connection,
			"SelectSources",
			g_variant_new("(oa{sv})", session_handle.c_str(), &options));
		GVariant* results = wait_for_portal_response(connection, handle);
		g_variant_unref(results);
	}

	PortalStreamInfo StartPortalSession(const std::string& session_handle)
	{
		GVariantBuilder options;
		g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token_for("openshot_start").c_str()));

		const std::string handle = call_portal_request(
			connection,
			"Start",
			g_variant_new("(osa{sv})", session_handle.c_str(), "", &options));
		GVariant* results = wait_for_portal_response(connection, handle);
		const PortalStreamInfo result_stream_info = stream_info_from_results(results);
		g_variant_unref(results);
		return result_stream_info;
	}

	void ApplyPortalStreamInfo()
	{
		if (stream_info.width <= 0 || stream_info.height <= 0) {
			return;
		}
		info.width = stream_info.width;
		info.height = stream_info.height;
		info.display_ratio = Fraction(stream_info.width, stream_info.height);
		info.display_ratio.Reduce();
	}

	void OpenPipeWireStream(int pipewire_fd)
	{
		pw_init(nullptr, nullptr);

		thread_loop = pw_thread_loop_new("openshot-wayland-capture", nullptr);
		if (!thread_loop) {
			throw InvalidOptions("Unable to create PipeWire thread loop.");
		}
		context = pw_context_new(pw_thread_loop_get_loop(thread_loop), nullptr, 0);
		if (!context) {
			throw InvalidOptions("Unable to create PipeWire context.");
		}
		core = pw_context_connect_fd(context, pipewire_fd, nullptr, 0);
		if (!core) {
			close(pipewire_fd);
			throw InvalidOptions("Unable to connect to the portal PipeWire remote.");
		}

		pw_properties* props = pw_properties_new(
			PW_KEY_MEDIA_TYPE, "Video",
			PW_KEY_MEDIA_CATEGORY, "Capture",
			PW_KEY_MEDIA_ROLE, "Screen",
			nullptr);
		if (stream_info.pipewire_serial > 0) {
			pw_properties_set(props, PW_KEY_TARGET_OBJECT, std::to_string(stream_info.pipewire_serial).c_str());
		}
		stream = pw_stream_new(core, "OpenShot Wayland Screen Capture", props);
		if (!stream) {
			throw InvalidOptions("Unable to create PipeWire capture stream.");
		}

		pw_stream_add_listener(stream, &stream_listener, &stream_events, this);

		if (pw_thread_loop_start(thread_loop) < 0) {
			throw InvalidOptions("Unable to start PipeWire capture thread loop.");
		}

		uint8_t format_buffer[1024];
		spa_pod_builder builder = SPA_POD_BUILDER_INIT(format_buffer, sizeof(format_buffer));
		const spa_fraction requested_framerate = SPA_FRACTION(
			static_cast<uint32_t>(std::max(1, settings.fps.num)),
			static_cast<uint32_t>(std::max(1, settings.fps.den)));
		const spa_fraction variable_framerate = SPA_FRACTION(0, 1);
		const spa_pod* params[1];
		params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
			&builder,
			SPA_TYPE_OBJECT_Format,
			SPA_PARAM_EnumFormat,
			SPA_FORMAT_mediaType,
			SPA_POD_Id(SPA_MEDIA_TYPE_video),
			SPA_FORMAT_mediaSubtype,
			SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
			SPA_FORMAT_VIDEO_format,
			SPA_POD_CHOICE_ENUM_Id(
				4,
				SPA_VIDEO_FORMAT_BGRx,
				SPA_VIDEO_FORMAT_RGBA,
				SPA_VIDEO_FORMAT_BGRA,
				SPA_VIDEO_FORMAT_RGBx),
			SPA_FORMAT_VIDEO_framerate,
			SPA_POD_Fraction(&variable_framerate),
			SPA_FORMAT_VIDEO_maxFramerate,
			SPA_POD_Fraction(&requested_framerate)));

		pw_thread_loop_lock(thread_loop);
		const uint32_t target_id = stream_info.pipewire_serial > 0 ? PW_ID_ANY : stream_info.node_id;
		const int result = pw_stream_connect(
			stream,
			PW_DIRECTION_INPUT,
			target_id,
			static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
			params,
			1);
		if (result < 0) {
			pw_thread_loop_unlock(thread_loop);
			throw InvalidOptions("Unable to connect to PipeWire capture stream.");
		}

		while (!streaming && !stream_error) {
			if (pw_thread_loop_timed_wait(thread_loop, 10) < 0) {
				stream_error = true;
				break;
			}
		}
		pw_thread_loop_unlock(thread_loop);

		if (stream_error) {
			throw InvalidOptions("PipeWire capture stream failed to start or timed out.");
		}
	}

	static void OnPortalSessionClosed(
		GDBusConnection*,
		const gchar*,
		const gchar*,
		const gchar*,
		const gchar*,
		GVariant*,
		gpointer user_data)
	{
		auto* self = static_cast<WaylandScreenCaptureReader*>(user_data);
		self->open = false;
		self->stream_error = true;
		if (self->thread_loop) {
			pw_thread_loop_signal(self->thread_loop, false);
		}
		self->queue_condition.notify_all();
	}

	static void OnStreamStateChanged(void* data, pw_stream_state, pw_stream_state state, const char*)
	{
		auto* self = static_cast<WaylandScreenCaptureReader*>(data);
		if (state == PW_STREAM_STATE_STREAMING) {
			self->streaming = true;
			pw_thread_loop_signal(self->thread_loop, false);
		} else if (state == PW_STREAM_STATE_ERROR || state == PW_STREAM_STATE_UNCONNECTED) {
			self->stream_error = true;
			pw_thread_loop_signal(self->thread_loop, false);
			self->queue_condition.notify_all();
		}
	}

	static void OnStreamParamChanged(void* data, uint32_t id, const spa_pod* param)
	{
		if (id != SPA_PARAM_Format || !param) {
			return;
		}
		auto* self = static_cast<WaylandScreenCaptureReader*>(data);
		spa_video_info info = {};
		if (spa_format_parse(param, &info.media_type, &info.media_subtype) < 0 ||
				info.media_type != SPA_MEDIA_TYPE_video ||
				info.media_subtype != SPA_MEDIA_SUBTYPE_raw ||
				spa_format_video_raw_parse(param, &info.info.raw) < 0) {
			return;
		}

		self->video_format = info.info.raw.format;
		self->stream_width = static_cast<int>(info.info.raw.size.width);
		self->stream_height = static_cast<int>(info.info.raw.size.height);
		if (info.info.raw.framerate.num > 0 && info.info.raw.framerate.denom > 0) {
			self->info.fps = Fraction(
				static_cast<int>(info.info.raw.framerate.num),
				static_cast<int>(info.info.raw.framerate.denom));
			self->info.video_timebase = self->info.fps.Reciprocal();
		}
		if (self->stream_width > 0 && self->stream_height > 0) {
			Logger::Instance()->Log(
				"Wayland PipeWire stream format: " +
				std::to_string(self->stream_width) + "x" + std::to_string(self->stream_height) +
				" format=" + std::to_string(self->video_format) +
					" framerate=" + std::to_string(info.info.raw.framerate.num) +
					"/" + std::to_string(info.info.raw.framerate.denom) +
					" max_framerate=" + std::to_string(info.info.raw.max_framerate.num) +
					"/" + std::to_string(info.info.raw.max_framerate.denom));
			self->info.width = self->stream_width;
			self->info.height = self->stream_height;
			self->info.display_ratio = Fraction(self->stream_width, self->stream_height);
			self->info.display_ratio.Reduce();
		}

		uint8_t params_buffer[1024];
		spa_pod_builder builder = SPA_POD_BUILDER_INIT(params_buffer, sizeof(params_buffer));
		const int stride = self->stream_width * 4;
		const int buffer_size = stride * self->stream_height;
		const spa_pod* params[3];
		params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
			&builder,
			SPA_TYPE_OBJECT_ParamBuffers,
			SPA_PARAM_Buffers,
			SPA_PARAM_BUFFERS_buffers,
			SPA_POD_CHOICE_RANGE_Int(8, 2, 32),
			SPA_PARAM_BUFFERS_blocks,
			SPA_POD_Int(1),
			SPA_PARAM_BUFFERS_size,
			SPA_POD_Int(buffer_size),
			SPA_PARAM_BUFFERS_stride,
			SPA_POD_Int(stride),
			SPA_PARAM_BUFFERS_dataType,
			SPA_POD_CHOICE_FLAGS_Int(
				(1 << SPA_DATA_MemPtr) | (1 << SPA_DATA_MemFd))));
		params[1] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
			&builder,
			SPA_TYPE_OBJECT_ParamMeta,
			SPA_PARAM_Meta,
			SPA_PARAM_META_type,
			SPA_POD_Id(SPA_META_Header),
			SPA_PARAM_META_size,
			SPA_POD_Int(sizeof(spa_meta_header))));
		params[2] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
			&builder,
			SPA_TYPE_OBJECT_ParamMeta,
			SPA_PARAM_Meta,
			SPA_PARAM_META_type,
			SPA_POD_Id(SPA_META_VideoCrop),
			SPA_PARAM_META_size,
			SPA_POD_Int(sizeof(spa_meta_region))));
		pw_stream_update_params(self->stream, params, 3);
	}

	static void OnStreamProcess(void* data)
	{
		auto* self = static_cast<WaylandScreenCaptureReader*>(data);
		// Drain the stream and process only the newest buffer. Returning stale
		// buffers immediately prevents a damage-driven window stream from
		// accumulating latency after it becomes visible again.
		pw_buffer* buffer = nullptr;
		pw_buffer* next_buffer = pw_stream_dequeue_buffer(self->stream);
		while (next_buffer) {
			if (buffer) {
				pw_stream_queue_buffer(self->stream, buffer);
				self->dropped_packets++;
			}
			buffer = next_buffer;
			next_buffer = pw_stream_dequeue_buffer(self->stream);
		}
		if (!buffer) {
			return;
		}

		self->CopyPipeWireBuffer(buffer);
		pw_stream_queue_buffer(self->stream, buffer);
	}

	void CopyPipeWireBuffer(pw_buffer* buffer)
	{
		spa_buffer* spa_buffer = buffer->buffer;
		if (!spa_buffer || spa_buffer->n_datas == 0 || !spa_buffer->datas[0].data || stream_width <= 0 || stream_height <= 0) {
			dropped_packets++;
			return;
		}

		auto* header = static_cast<spa_meta_header*>(
			spa_buffer_find_meta_data(spa_buffer, SPA_META_Header, sizeof(spa_meta_header)));
		if (header && !AcceptHeader(*header)) {
			dropped_packets++;
			return;
		}

		spa_data& data = spa_buffer->datas[0];
		const spa_chunk* chunk = data.chunk;
		int crop_x = 0;
		int crop_y = 0;
		int crop_width = stream_width;
		int crop_height = stream_height;
		auto* crop = static_cast<spa_meta_region*>(
			spa_buffer_find_meta_data(spa_buffer, SPA_META_VideoCrop, sizeof(spa_meta_region)));
		if (crop && spa_meta_region_is_valid(crop)) {
			crop_x = std::max(0, crop->region.position.x);
			crop_y = std::max(0, crop->region.position.y);
			crop_width = std::min(static_cast<int>(crop->region.size.width), stream_width - crop_x);
			crop_height = std::min(static_cast<int>(crop->region.size.height), stream_height - crop_y);
			if (!crop_logged) {
				Logger::Instance()->Log(
					"Wayland PipeWire video crop: x=" + std::to_string(crop_x) +
					" y=" + std::to_string(crop_y) +
					" width=" + std::to_string(crop_width) +
					" height=" + std::to_string(crop_height));
				crop_logged = true;
			}
		}

		const auto layout = wayland::ResolvePackedVideoLayout(
			static_cast<size_t>(data.maxsize),
			chunk ? static_cast<size_t>(chunk->offset) : 0,
			chunk ? static_cast<size_t>(chunk->size) : static_cast<size_t>(data.maxsize),
			chunk ? chunk->stride : 0,
			stream_width,
			stream_height,
			crop_x,
			crop_y,
			crop_width,
			crop_height);
		if (!layout.valid) {
			dropped_packets++;
			return;
		}

		CapturedFrame frame;
		frame.width = layout.width;
		frame.height = layout.height;
		frame.rgba.assign(static_cast<size_t>(frame.width) * frame.height * 4, 0);

		std::vector<uint8_t> source_row(static_cast<size_t>(frame.width) * 4);
		for (int y = 0; y < frame.height; ++y) {
			const size_t logical_offset =
				static_cast<size_t>(y + layout.crop_y) * layout.stride
				+ static_cast<size_t>(layout.crop_x) * 4;
			if (!wayland::CopyWrappedBytes(
					static_cast<const uint8_t*>(data.data),
					static_cast<size_t>(data.maxsize),
					layout.offset,
					logical_offset,
					source_row.data(),
					source_row.size())) {
				dropped_packets++;
				return;
			}
			for (int x = 0; x < frame.width; ++x) {
				const uint8_t* pixel = source_row.data() + static_cast<size_t>(x) * 4;
				uint8_t r = 0;
				uint8_t g = 0;
				uint8_t b = 0;
				uint8_t a = 255;
				switch (video_format) {
				case SPA_VIDEO_FORMAT_RGBA:
					r = pixel[0]; g = pixel[1]; b = pixel[2]; a = pixel[3];
					break;
				case SPA_VIDEO_FORMAT_RGBx:
					r = pixel[0]; g = pixel[1]; b = pixel[2];
					break;
				case SPA_VIDEO_FORMAT_BGRA:
					b = pixel[0]; g = pixel[1]; r = pixel[2]; a = pixel[3];
					break;
				case SPA_VIDEO_FORMAT_BGRx:
				default:
					b = pixel[0]; g = pixel[1]; r = pixel[2];
					break;
				}
				const size_t dst = (static_cast<size_t>(y) * crop_width + x) * 4;
				frame.rgba[dst + 0] = r;
				frame.rgba[dst + 1] = g;
				frame.rgba[dst + 2] = b;
				frame.rgba[dst + 3] = a;
			}
		}

		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			if (frame_queue.size() > 3) {
				frame_queue.pop_front();
				dropped_packets++;
			}
			frame_queue.push_back(std::move(frame));
		}
		if (info.width != frame.width || info.height != frame.height) {
			info.width = frame.width;
			info.height = frame.height;
			info.display_ratio = Fraction(frame.width, frame.height);
			info.display_ratio.Reduce();
		}
		queue_condition.notify_one();
	}

	bool AcceptHeader(const spa_meta_header& header)
	{
		if (header.flags & (SPA_META_HEADER_FLAG_CORRUPTED | SPA_META_HEADER_FLAG_GAP)) {
			LogDroppedHeader("flags", header);
			return false;
		}

		const bool discontinuity = (header.flags & SPA_META_HEADER_FLAG_DISCONT) != 0;
		if (!discontinuity && have_last_header_sequence && header.seq > last_header_sequence) {
			header_sequence_ordering_active = true;
		}
		if (!discontinuity && header_sequence_ordering_active && header.seq <= last_header_sequence) {
			LogDroppedHeader("sequence", header);
			return false;
		}
		if (!discontinuity && header.pts >= 0 && have_last_header_pts && header.pts < last_header_pts) {
			LogDroppedHeader("pts", header);
			return false;
		}

		have_last_header_sequence = true;
		last_header_sequence = header.seq;
		if (header.pts >= 0) {
			have_last_header_pts = true;
			last_header_pts = header.pts;
		}
		return true;
	}

	void LogDroppedHeader(const std::string& reason, const spa_meta_header& header)
	{
		if (header_drop_log_count >= 5) {
			return;
		}
		Logger::Instance()->Log(
			"Wayland PipeWire dropped non-monotonic frame: reason=" + reason +
			" flags=" + std::to_string(header.flags) +
			" seq=" + std::to_string(header.seq) +
			" last_seq=" + (have_last_header_sequence ? std::to_string(last_header_sequence) : std::string("none")) +
			" seq_active=" + std::to_string(header_sequence_ordering_active ? 1 : 0) +
			" pts=" + std::to_string(header.pts) +
			" last_pts=" + (have_last_header_pts ? std::to_string(last_header_pts) : std::string("none")));
		header_drop_log_count++;
	}

	ScreenCaptureSettings settings;
	ReaderInfo& info;
	GDBusConnection* connection;
	pw_thread_loop* thread_loop;
	pw_context* context;
	pw_core* core;
	pw_stream* stream;
	spa_hook stream_listener;
	PortalStreamInfo stream_info;
	std::string session_handle;
	guint session_closed_subscription;
	int stream_width = 0;
	int stream_height = 0;
	spa_video_format video_format = SPA_VIDEO_FORMAT_BGRx;
	int64_t frames_read;
	int dropped_packets;
	bool open;
	bool streaming;
	bool stream_error;
	bool crop_logged;
	bool have_last_header_sequence;
	uint64_t last_header_sequence;
	bool header_sequence_ordering_active;
	bool have_last_header_pts;
	int64_t last_header_pts;
	int header_drop_log_count;
	std::mutex queue_mutex;
	std::condition_variable queue_condition;
	std::deque<CapturedFrame> frame_queue;
	CapturedFrame last_frame;
	bool have_last_frame = false;

	static const pw_stream_events stream_events;
};

const pw_stream_events WaylandScreenCaptureReader::stream_events = {
	PW_VERSION_STREAM_EVENTS,
	nullptr,
	WaylandScreenCaptureReader::OnStreamStateChanged,
	nullptr,
	nullptr,
	WaylandScreenCaptureReader::OnStreamParamChanged,
	nullptr,
	nullptr,
	WaylandScreenCaptureReader::OnStreamProcess,
	nullptr,
	nullptr,
	nullptr
};

extern "C" std::unique_ptr<ScreenCaptureReader::CaptureBackendReader> OpenShotCreateWaylandScreenCaptureReader(
	const ScreenCaptureSettings& settings,
	ReaderInfo& info)
{
	return std::make_unique<WaylandScreenCaptureReader>(settings, info);
}
