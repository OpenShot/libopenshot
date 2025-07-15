/**
 * @file
 * @brief Header file for global Settings class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_SETTINGS_H
#define OPENSHOT_SETTINGS_H

#include <string>

namespace openshot {

	/**
	 * @brief This class is contains settings used by libopenshot (and can be safely toggled at any point)
	 *
	 * Settings class is used primarily to toggle scale settings between preview and rendering, and adjust
	 * other runtime related settings.
	 */
	class Settings {
	private:

		/// Default constructor
		Settings(){}; 						 // Don't allow user to create an instance of this singleton

#if __GNUC__ >=7
		/// Default copy method
		Settings(Settings const&) = delete; // Don't allow the user to assign this instance

		/// Default assignment operator
		Settings & operator=(Settings const&) = delete;  // Don't allow the user to assign this instance
#else
		/// Default copy method
		Settings(Settings const&) {}; // Don't allow the user to assign this instance

		/// Default assignment operator
		Settings & operator=(Settings const&);  // Don't allow the user to assign this instance
#endif

		/// Private variable to keep track of singleton instance
		static Settings * m_pInstance;

	public:
		/**
		 * @brief Use video codec for faster video decoding (if supported)
		 *
		 * 0 - No acceleration,
		 * 1 - Linux VA-API,
		 * 2 - nVidia NVDEC,
		 * 3 - Windows D3D9,
		 * 4 - Windows D3D11,
		 * 5 - MacOS / VideoToolBox,
		 * 6 - Linux VDPAU,
		 * 7 - Intel QSV
		 */
		int HARDWARE_DECODER = 0;

		/// Scale mode used in FFmpeg decoding and encoding (used as an optimization for faster previews)
		bool HIGH_QUALITY_SCALING = false;

		/// Number of threads of OpenMP
		int OMP_THREADS = 16;

		/// Number of threads that ffmpeg uses
		int FF_THREADS = 16;

		/// Maximum rows that hardware decode can handle
		int DE_LIMIT_HEIGHT_MAX = 1100;

		/// Maximum columns that hardware decode can handle
		int DE_LIMIT_WIDTH_MAX = 1950;

		/// Which GPU to use to decode (0 is the first)
		int HW_DE_DEVICE_SET = 0;

		/// Which GPU to use to encode (0 is the first)
		int HW_EN_DEVICE_SET = 0;

		/// Percentage of cache in front of the playhead (0.0 to 1.0)
		float VIDEO_CACHE_PERCENT_AHEAD = 0.7;

		/// Minimum number of frames to cache before playback begins
		int VIDEO_CACHE_MIN_PREROLL_FRAMES = 24;

		/// Max number of frames (ahead of playhead) to cache during playback
		int VIDEO_CACHE_MAX_PREROLL_FRAMES = 48;

		/// Max number of frames (when paused) to cache for playback
		int VIDEO_CACHE_MAX_FRAMES = 30 * 10;

		/// Enable/Disable the cache thread to pre-fetch and cache video frames before we need them
		bool ENABLE_PLAYBACK_CACHING = true;

		/// The audio device name to use during playback
		std::string PLAYBACK_AUDIO_DEVICE_NAME = "";

		/// The device type for the playback audio devices
		std::string PLAYBACK_AUDIO_DEVICE_TYPE = "";

		/// Size of playback buffer before audio playback starts
		int PLAYBACK_AUDIO_BUFFER_SIZE = 512;

		/// The current install path of OpenShot (needs to be set when using Timeline(path), since certain
		/// paths depend on the location of OpenShot transitions and files)
		std::string PATH_OPENSHOT_INSTALL = "";

 		/// Whether to dump ZeroMQ debug messages to stderr
		bool DEBUG_TO_STDERR = false;

		/// Create or get an instance of this logger singleton (invoke the class with this method)
		static Settings * Instance();
	};

}

#endif
