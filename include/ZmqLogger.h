/**
 * @file
 * @brief Header file for ZeroMQ-based Logger class
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

#ifndef OPENSHOT_LOGGER_H
#define OPENSHOT_LOGGER_H


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
	 * @brief This abstract class is the base class, used by all readers in libopenshot.
	 *
	 * Readers are types of classes that read video, audio, and image files, and
	 * return openshot::Frame objects. The only requirements for a 'reader', are to
	 * derive from this base class, implement the GetFrame method, and call the InitFileInfo() method.
	 */
	class ZmqLogger {
	private:
		CriticalSection loggerCriticalSection;
		string connection;

		// Logfile related vars
		string file_path;
		ofstream log_file;
		bool enabled;

		/// ZMQ Context
		zmq::context_t *context;

		/// ZMQ Socket
		zmq::socket_t *publisher;

		/// Default constructor
		ZmqLogger(){}; 						 // Don't allow user to create an instance of this singleton

		/// Default copy method
		ZmqLogger(ZmqLogger const&){};             // Don't allow the user to copy this instance

		/// Default assignment operator
		ZmqLogger & operator=(ZmqLogger const&){};  // Don't allow the user to assign this instance

		/// Private variable to keep track of singleton instance
		static ZmqLogger * m_pInstance;

	public:
		/// Create or get an instance of this logger singleton (invoke the class with this method)
		static ZmqLogger * Instance();

		/// Append debug information
		void AppendDebugMethod(string method_name, string arg1_name, float arg1_value,
							   string arg2_name, float arg2_value,
							   string arg3_name, float arg3_value,
							   string arg4_name, float arg4_value,
							   string arg5_name, float arg5_value,
							   string arg6_name, float arg6_value);

		/// Close logger (sockets and/or files)
		void Close();

		/// Set or change connection info for logger (i.e. tcp://*:5556)
		void Connection(string new_connection);

		/// Enable/Disable logging
		void Enable(bool is_enabled) { enabled = is_enabled;};

		/// Set or change the file path (optional)
		void Path(string new_path);

		/// Log message to all subscribers of this logger (if any)
		void Log(string message);

		/// Log message to a file (if path set)
		void LogToFile(string message);
	};

}

#endif
