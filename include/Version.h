/**
 * @file
 * @brief Header file that includes the version number of libopenshot
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

#ifndef OPENSHOT_VERSION_H
#define OPENSHOT_VERSION_H

// Crazy c++ macro to convert an integer into a string
#ifndef STRINGIZE
	#define STRINGIZE_(x) #x
	#define STRINGIZE(x) STRINGIZE_(x)
#endif

#define OPENSHOT_VERSION_MAJOR 0;   /// Major version number is incremented when huge features are added or improved.
#define OPENSHOT_VERSION_MINOR 1;   /// Minor version is incremented when smaller (but still very important) improvements are added.
#define OPENSHOT_VERSION_BUILD 8;   /// Build number is incremented when minor bug fixes and less important improvements are added.
#define OPENSHOT_VERSION_SO 13;     /// Shared object version number. This increments any time the API and ABI changes (so old apps will no longer link)
#define OPENSHOT_VERSION_MAJOR_MINOR STRINGIZE(OPENSHOT_VERSION_MAJOR) "." STRINGIZE(OPENSHOT_VERSION_MINOR); /// A string of the "Major.Minor" version
#define OPENSHOT_VERSION_ALL STRINGIZE(OPENSHOT_VERSION_MAJOR) "." STRINGIZE(OPENSHOT_VERSION_MINOR) "." STRINGIZE(OPENSHOT_VERSION_BUILD); /// A string of the entire version "Major.Minor.Build"

#include <sstream>
using namespace std;

namespace openshot
{
	/// This struct holds version number information. Use the GetVersion() method to access the current version of libopenshot.
	struct OpenShotVersion {
		int major; /// Major version number
		int minor; /// Minor version number
		int build; /// Build number
		int so; /// Shared Object Number (incremented when API or ABI changes)

		/// Get a string version of the version (i.e. "Major.Minor.Build")
		string ToString() {
			stringstream version_string;
			version_string << major << "." << minor << "." << build;
			return version_string.str();
		}
	};

	/// Get the current version number of libopenshot (major, minor, and build number)
	static OpenShotVersion GetVersion() {
		OpenShotVersion version;

		// Set version info
		version.major = OPENSHOT_VERSION_MAJOR;
		version.minor = OPENSHOT_VERSION_MINOR;
		version.build = OPENSHOT_VERSION_BUILD;
		version.so = OPENSHOT_VERSION_SO;

		return version;
	}
}
#endif
