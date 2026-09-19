/**
 * @file
 * @brief Source file for live FFmpeg camera capture readers
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "CameraCaptureReader.h"

#if defined(__linux__)
#include "CameraCaptureV4L2.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#include <cstdlib>
#include <string>

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <dshow.h>
	#include <oleauto.h>
#endif

extern "C" {
	#include <libavdevice/avdevice.h>
}

#include "Exceptions.h"

using namespace openshot;

#if defined(_WIN32)
namespace
{
	std::string WideToUtf8(const wchar_t* value)
	{
		if (!value || !*value) {
			return "";
		}
		const int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
		if (length <= 1) {
			return "";
		}
		std::string converted(static_cast<size_t>(length - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, value, -1, &converted[0], length, nullptr, nullptr);
		return converted;
	}

	bool ContainsDeviceName(const AudioDeviceList& devices, const std::string& name)
	{
		for (const auto& device : devices) {
			if (device.second == name) {
				return true;
			}
		}
		return false;
	}

	AudioDeviceList GetWindowsDirectShowVideoDevices()
	{
		AudioDeviceList devices;
		const HRESULT init_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		const bool should_uninitialize = SUCCEEDED(init_result);
		if (FAILED(init_result) && init_result != RPC_E_CHANGED_MODE) {
			return devices;
		}

		ICreateDevEnum* device_enumerator = nullptr;
		HRESULT result = CoCreateInstance(
			CLSID_SystemDeviceEnum,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_ICreateDevEnum,
			reinterpret_cast<void**>(&device_enumerator)
		);
		if (SUCCEEDED(result) && device_enumerator) {
			IEnumMoniker* moniker_enumerator = nullptr;
			result = device_enumerator->CreateClassEnumerator(
				CLSID_VideoInputDeviceCategory,
				&moniker_enumerator,
				0
			);
			if (result == S_OK && moniker_enumerator) {
				IMoniker* moniker = nullptr;
				while (moniker_enumerator->Next(1, &moniker, nullptr) == S_OK) {
					IPropertyBag* property_bag = nullptr;
					result = moniker->BindToStorage(
						nullptr,
						nullptr,
						IID_IPropertyBag,
						reinterpret_cast<void**>(&property_bag)
					);
					if (SUCCEEDED(result) && property_bag) {
						VARIANT friendly_name;
						VariantInit(&friendly_name);
						result = property_bag->Read(L"FriendlyName", &friendly_name, nullptr);
						if (SUCCEEDED(result) && friendly_name.vt == VT_BSTR) {
							const std::string name = WideToUtf8(friendly_name.bstrVal);
							if (!name.empty() && !ContainsDeviceName(devices, name)) {
								devices.emplace_back(name, name);
							}
						}
						VariantClear(&friendly_name);
						property_bag->Release();
					}
					moniker->Release();
					moniker = nullptr;
				}
				moniker_enumerator->Release();
			}
			device_enumerator->Release();
		}
		if (should_uninitialize) {
			CoUninitialize();
		}
		return devices;
	}
}
#endif

CameraCaptureReader::CameraCaptureReader(const CameraCaptureSettings& new_settings)
	: settings(new_settings)
	, reader(nullptr)
{
	if (settings.backend == CAMERA_CAPTURE_AUTO) {
		settings.backend = DefaultBackend();
	}
	ValidateSettings();
	RebuildReader();
}

CameraCaptureReader::~CameraCaptureReader()
{
	Close();
}

bool CameraCaptureReader::IsBackendSupported(CameraCaptureBackend backend)
{
#if defined(__linux__)
	return backend == CAMERA_CAPTURE_V4L2 || backend == CAMERA_CAPTURE_AUTO;
#elif defined(_WIN32)
	return backend == CAMERA_CAPTURE_WINDOWS_DSHOW || backend == CAMERA_CAPTURE_AUTO;
#elif defined(__APPLE__)
	return backend == CAMERA_CAPTURE_MAC_AVFOUNDATION || backend == CAMERA_CAPTURE_AUTO;
#else
	(void) backend;
	return false;
#endif
}

CameraCaptureBackend CameraCaptureReader::DefaultBackend()
{
#if defined(__linux__)
	return CAMERA_CAPTURE_V4L2;
#elif defined(_WIN32)
	return CAMERA_CAPTURE_WINDOWS_DSHOW;
#elif defined(__APPLE__)
	return CAMERA_CAPTURE_MAC_AVFOUNDATION;
#else
	return CAMERA_CAPTURE_AUTO;
#endif
}

AudioDeviceList CameraCaptureReader::GetDeviceNames(CameraCaptureBackend backend)
{
	if (backend == CAMERA_CAPTURE_AUTO) {
		backend = DefaultBackend();
	}

	AudioDeviceList devices;
	const char* input_format_name = nullptr;
#if defined(__linux__)
	if (backend == CAMERA_CAPTURE_V4L2) {
		input_format_name = "v4l2";
	}
#elif defined(_WIN32)
	if (backend == CAMERA_CAPTURE_WINDOWS_DSHOW) {
		return GetWindowsDirectShowVideoDevices();
	}
#elif defined(__APPLE__)
	if (backend == CAMERA_CAPTURE_MAC_AVFOUNDATION) {
		input_format_name = "avfoundation";
	}
#endif
	if (!input_format_name) {
		return devices;
	}

	avdevice_register_all();
	AVInputFormat* input_format = const_cast<AVInputFormat*>(av_find_input_format(input_format_name));
	if (!input_format) {
		return devices;
	}

	AVDeviceInfoList* device_list = nullptr;
	const int result = avdevice_list_input_sources(input_format, nullptr, nullptr, &device_list);
	if (result >= 0 && device_list) {
		for (int index = 0; index < device_list->nb_devices; ++index) {
			const AVDeviceInfo* device = device_list->devices[index];
			if (!device || !device->device_name) {
				continue;
			}
			const std::string name = device->device_name;
			const std::string label = device->device_description
				? device->device_description
				: device->device_name;
		#if defined(__APPLE__)
			if (backend == CAMERA_CAPTURE_MAC_AVFOUNDATION && label.find("Capture screen") != std::string::npos) {
				continue;
			}
		#endif
			devices.emplace_back(label, name);
		}
	}
	avdevice_free_list_devices(&device_list);
	return devices;
}

std::vector<CameraCaptureMode> CameraCaptureReader::GetDeviceModes(
    const std::string& device, CameraCaptureBackend backend)
{
    if (backend == CAMERA_CAPTURE_AUTO) backend = DefaultBackend();
#if defined(__linux__)
    if (backend == CAMERA_CAPTURE_V4L2) {
        const int fd = open(device.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) throw InvalidFile("Unable to open camera for mode discovery.", device);
        struct DeviceHandle {
            int fd;
            ~DeviceHandle() { close(fd); }
        } handle{fd};
        return detail::EnumerateCameraModes([fd](unsigned long request, void* value) {
            return ioctl(fd, request, value);
        });
    }
#else
    (void) device;
#endif
    return {};
}

void CameraCaptureReader::ValidateSettings() const
{
	if (!IsBackendSupported(settings.backend)) {
		throw InvalidOptions("Camera capture backend is not supported on this OS.");
	}
	if (settings.backend != CAMERA_CAPTURE_V4L2
		&& settings.backend != CAMERA_CAPTURE_WINDOWS_DSHOW
		&& settings.backend != CAMERA_CAPTURE_MAC_AVFOUNDATION) {
		throw InvalidOptions("Camera capture backend is not implemented in this build.");
	}
	if (settings.device.empty()) {
		throw InvalidOptions("Camera capture requires a device path.");
	}
	if (settings.width <= 0 || settings.height <= 0) {
		throw InvalidOptions("Camera capture requires a positive width and height.");
	}
	if (settings.fps.num <= 0 || settings.fps.den <= 0) {
		throw InvalidOptions("Camera capture requires a positive frame rate.");
	}
}

ScreenCaptureSettings CameraCaptureReader::ToDeviceSettings() const
{
	ScreenCaptureSettings converted;
	converted.display = settings.device;
	converted.width = settings.width;
	converted.height = settings.height;
	converted.fps = settings.fps;
	converted.options = settings.options;
	if (settings.backend == CAMERA_CAPTURE_WINDOWS_DSHOW) {
		converted.backend = SCREEN_CAPTURE_WINDOWS_GDI;
		converted.display = "video=" + settings.device;
		converted.options["input_format_name"] = "dshow";
	} else if (settings.backend == CAMERA_CAPTURE_MAC_AVFOUNDATION) {
		converted.backend = SCREEN_CAPTURE_MAC_AVFOUNDATION;
		converted.display = settings.device.find(':') == std::string::npos
			? settings.device + ":none"
			: settings.device;
		converted.options["input_format_name"] = "avfoundation";
	} else {
		converted.backend = SCREEN_CAPTURE_X11;
		converted.options["input_format_name"] = "v4l2";
	}
	return converted;
}

void CameraCaptureReader::RebuildReader()
{
	reader = std::make_unique<ScreenCaptureReader>(ToDeviceSettings());
	info = reader->info;
	info.metadata["capture_type"] = "camera";
}

void CameraCaptureReader::Open()
{
	reader->Open();
	info = reader->info;
	info.metadata["capture_type"] = "camera";
}

void CameraCaptureReader::Close()
{
	if (reader) {
		reader->Close();
	}
}

openshot::CaptureReaderStats CameraCaptureReader::GetStats() const
{
	return reader ? reader->GetStats() : CaptureReaderStats();
}

std::shared_ptr<Frame> CameraCaptureReader::GetFrame(int64_t number)
{
	return reader->GetFrame(number);
}

std::string CameraCaptureReader::Json() const
{
	return JsonValue().toStyledString();
}

Json::Value CameraCaptureReader::JsonValue() const
{
	Json::Value root = ReaderBase::JsonValue();
	root["type"] = "CameraCaptureReader";
	root["backend"] = settings.backend;
	root["device"] = settings.device;
	root["width"] = settings.width;
	root["height"] = settings.height;
	root["fps"]["num"] = settings.fps.num;
	root["fps"]["den"] = settings.fps.den;
	root["options"] = Json::Value(Json::objectValue);
	for (const auto& option : settings.options) {
		root["options"][option.first] = option.second;
	}
	return root;
}

void CameraCaptureReader::SetJson(const std::string value)
{
	try {
		SetJsonValue(openshot::stringToJson(value));
	} catch (const std::exception&) {
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void CameraCaptureReader::SetJsonValue(const Json::Value root)
{
	if (!root["backend"].isNull())
		settings.backend = static_cast<CameraCaptureBackend>(root["backend"].asInt());
	if (!root["device"].isNull())
		settings.device = root["device"].asString();
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
	if (!root["options"].isNull() && root["options"].isObject()) {
		settings.options.clear();
		for (const auto& key : root["options"].getMemberNames()) {
			settings.options[key] = root["options"][key].asString();
		}
	}
	if (settings.backend == CAMERA_CAPTURE_AUTO) {
		settings.backend = DefaultBackend();
	}
	ValidateSettings();
	RebuildReader();
}
