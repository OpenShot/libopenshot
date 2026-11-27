/**
 * @file
 * @brief Native macOS camera discovery
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#import <AVFoundation/AVFoundation.h>

#include "AudioDevices.h"

// Legacy APIs remain necessary for older deployment targets.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace openshot {
AudioDeviceList GetMacAVFoundationVideoDevices()
{
	AudioDeviceList devices;
	@autoreleasepool {
		NSArray<AVCaptureDevice*>* cameras = nil;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
		if (@available(macOS 10.15, *)) {
			NSArray<AVCaptureDeviceType>* types = @[
				AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternalUnknown];
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
			if (@available(macOS 14.0, *)) {
				types = @[AVCaptureDeviceTypeBuiltInWideAngleCamera,
					AVCaptureDeviceTypeExternal, AVCaptureDeviceTypeContinuityCamera];
			}
#endif
			cameras = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:types
				mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionUnspecified].devices;
		} else
#endif
		{
			cameras = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
		}

		// Discover only; normal capture open handles authorization.
		for (AVCaptureDevice* camera in cameras) {
			const char* name = camera.localizedName.UTF8String;
			if (name && *name) {
				// FFmpeg accepts localized names, not native discovery indices or IDs.
				// Same-name cameras share FFmpeg's first-match behavior.
				devices.emplace_back(name, name);
			}
		}
	}
	return devices;
}
}

#pragma clang diagnostic pop
