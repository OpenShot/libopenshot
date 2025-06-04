/**
 * @file
 * @brief Header file for Profile class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PROFILE_H
#define OPENSHOT_PROFILE_H

#include <iostream>
#include <string>
#include <math.h>
#include <fstream>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QFile>
#include <QTextStream>
#include <cstdio>
#include <cstdlib>
#include "Fraction.h"
#include "Json.h"

namespace openshot
{

	/**
	 * @brief This struct holds profile data, typically loaded from a file
	 *
	 * Profile data contains common settings for Writers, such as frame rate,
	 * aspect ratios, width, and height combinations.
	 */
	struct ProfileInfo
	{
		std::string description;	///< The description of this profile.
		int height;		///< The height of the video (in pixels)
		int width;		///< The width of the video (in pixels)
		int pixel_format;	///< The pixel format (i.e. YUV420P, RGB24, etc...)
		Fraction fps;		///< Frames per second, as a fraction (i.e. 24/1 = 24 fps)
		Fraction pixel_ratio;	///< The pixel ratio of the video stream as a fraction (i.e. some pixels are not square)
		Fraction display_ratio;	///< The ratio of width to height of the video stream (i.e. 640x480 has a ratio of 4/3)
		bool interlaced_frame;	///< Are the contents of this frame interlaced
		bool spherical;		///< Is this video a spherical/360° video
	};

	/**
	 * @brief This class loads a special text-based file called a Profile.
	 *
	 * Profile data contains common video settings, such as framerate, height, width,
	 * aspect ratio, etc... All derived classes from openshot::WriterBase can load profile
	 * data using this class.
	 *
	 * \code
	 * // This example demonstrates how to load a profile with this class.
	 * Profile p("/home/jonathan/dv_ntsc_wide"); // Load the DV NTSC Widt profile data.
	 *
	 * \endcode
	 */
	class Profile
	{
	private:
		std::string formattedFPS(bool include_decimal); ///< Return a formatted FPS

		/// Less than operator (compare profile objects)
		/// Compare # of pixels, then FPS, then DAR
		friend bool operator<(const Profile& l, const Profile& r)
		{
			double left_fps = l.info.fps.ToDouble();
			double right_fps = r.info.fps.ToDouble();
			double left_pixels = l.info.width * l.info.height;
			double right_pixels = r.info.width * r.info.height;
			double left_dar = l.info.display_ratio.ToDouble();
			double right_dar = r.info.display_ratio.ToDouble();

			if (left_pixels < right_pixels) {
				// less pixels
				return true;
			} else {
				if (left_fps < right_fps) {
					// less FPS
					return true;
				} else {
					if (left_dar < right_dar) {
						// less DAR
						return true;
					} else {
						return false;
					}
				}
			}
		}

		/// Greater than operator (compare profile objects)
		/// Compare # of pixels, then FPS, then DAR
		friend bool operator>(const Profile& l, const Profile& r)
		{
			double left_fps = l.info.fps.ToDouble();
			double right_fps = r.info.fps.ToDouble();
			double left_pixels = l.info.width * l.info.height;
			double right_pixels = r.info.width * r.info.height;
			double left_dar = l.info.display_ratio.ToDouble();
			double right_dar = r.info.display_ratio.ToDouble();

			if (left_pixels > right_pixels) {
				// less pixels
				return true;
			} else {
				if (left_fps > right_fps) {
					// less FPS
					return true;
				} else {
					if (left_dar > right_dar) {
						// less DAR
						return true;
					} else {
						return false;
					}
				}
			}
		}

		/// Equality operator (compare profile objects)
		friend bool operator==(const Profile& l, const Profile& r)
		{
			return std::tie(l.info.width, l.info.height, l.info.fps.num, l.info.fps.den, l.info.display_ratio.num, l.info.display_ratio.den, l.info.interlaced_frame, l.info.spherical)
				   == std::tie(r.info.width, r.info.height, r.info.fps.num, r.info.fps.den, r.info.display_ratio.num, r.info.display_ratio.den, r.info.interlaced_frame, r.info.spherical);
		}

	public:
		/// Profile data stored here
		ProfileInfo info;

		/// @brief Default Constructor for Profile.
		Profile();
		
		/// @brief Constructor for Profile.
		/// @param path 	The file path / location of a profile file
		Profile(std::string path);

		/// @brief Save profile to a text file (label=value, one per line format)
		/// @param file_path 	The file path / location of a profile file
		void Save(const std::string& file_path) const;

		std::string Key(); ///< Return a unique key of this profile with padding (01920x1080i2997_16:09)
		std::string ShortName(); ///< Return the name of this profile (1920x1080p29.97)
		std::string LongName(); ///< Return a longer format name (1920x1080p @ 29.97 fps (16:9))
		std::string LongNameWithDesc(); ///< Return a longer format name with description (1920x1080p @ 29.97 fps (16:9) HD 1080i 29.97 fps)

		// Get and Set JSON methods
		std::string Json() const; ///< Generate JSON string of this object
		Json::Value JsonValue() const; ///< Generate Json::Value for this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
	};

}

#endif