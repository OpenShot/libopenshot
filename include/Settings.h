/**
 * @file
 * @brief Header file for global Settings class
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

#ifndef OPENSHOT_SETTINGS_H
#define OPENSHOT_SETTINGS_H


#include "JuceLibraryCode/JuceHeader.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <stdio.h>
#include <time.h>
#include <zmq.hpp>
#include <unistd.h>


using namespace std;

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
		/// Use video card for faster video decoding (if supported)
		bool HARDWARE_DECODE = false;

		/// Use video card for faster video encoding (if supported)
		bool HARDWARE_ENCODE = false;

		/// Scale mode used in FFmpeg decoding and encoding (used as an optimization for faster previews)
		bool HIGH_QUALITY_SCALING = false;

		/// Maximum width for image data (useful for optimzing for a smaller preview or render)
		int MAX_WIDTH = 0;

		/// Maximum height for image data (useful for optimzing for a smaller preview or render)
		int MAX_HEIGHT = 0;

		/// Wait for OpenMP task to finish before continuing (used to limit threads on slower systems)
		bool WAIT_FOR_VIDEO_PROCESSING_TASK = false;

		/// Create or get an instance of this logger singleton (invoke the class with this method)
		static Settings * Instance();
	};

}

#endif
