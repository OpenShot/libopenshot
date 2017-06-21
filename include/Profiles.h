/**
 * @file
 * @brief Header file for Profile class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_PROFILE_H
#define OPENSHOT_PROFILE_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qfile.h>
#include <QTextStream>
#include <stdio.h>
#include <stdlib.h>
#include "Exceptions.h"
#include "Fraction.h"
#include "Json.h"

using namespace std;

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
		string description;	///< The description of this profile.
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
		Profile(string path) throw(InvalidFile, InvalidJSON);

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object
	};

}

#endif
