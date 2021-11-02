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
#include <sstream>
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
		bool interlaced_frame;	// Are the contents of this frame interlaced
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
	public:
		/// Profile data stored here
		ProfileInfo info;

		/// @brief Constructor for Profile.
		/// @param path 	The folder path / location of a profile file
		Profile(std::string path);

		// Get and Set JSON methods
		std::string Json() const; ///< Generate JSON string of this object
		Json::Value JsonValue() const; ///< Generate Json::Value for this object
		void SetJson(const std::string value); ///< Load JSON string into this object
		void SetJsonValue(const Json::Value root); ///< Load Json::Value into this object
	};

}

#endif
