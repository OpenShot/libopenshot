/**
 * @file
 * @brief Header file for ZeroMQ-based Logger class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_LOGGER_H
#define OPENSHOT_LOGGER_H


#include <fstream>
#include <memory>
#include <mutex>
#include <string>

#include <zmq.hpp>

namespace openshot {

	/**
	 * @brief This class is used for logging and sending those logs over a ZemoMQ socket to a listener
	 *
	 * OpenShot desktop editor listens to this port, to receive libopenshot debug output. It both logs to
	 * a file and sends the stdout over a socket.
	 */
	class ZmqLogger {
	private:
		std::recursive_mutex loggerMutex;
		std::string connection;

		// Logfile related vars
		std::string file_path;
		std::ofstream log_file;
		bool enabled;

		/// ZMQ Context
		zmq::context_t *context;

		/// ZMQ Socket
		zmq::socket_t *publisher;

		/// Default constructor
		ZmqLogger(){};  // Don't allow user to create an instance of this singleton

#if __GNUC__ >=7
		/// Default copy method
		ZmqLogger(ZmqLogger const&) = delete;  // Don't allow the user to assign this instance

		/// Default assignment operator
		ZmqLogger & operator=(ZmqLogger const&) = delete;  // Don't allow the user to assign this instance
#else
		/// Default copy method
		ZmqLogger(ZmqLogger const&) {};  // Don't allow the user to assign this instance

		/// Default assignment operator
		ZmqLogger & operator=(ZmqLogger const&);  // Don't allow the user to assign this instance
#endif

		/// Private variable to keep track of singleton instance
		static ZmqLogger * m_pInstance;

	public:
		/// Create or get an instance of this logger singleton (invoke the class with this method)
		static ZmqLogger * Instance();

		/// Append debug information
		void AppendDebugMethod(
			std::string method_name,
			std::string arg1_name="", float arg1_value=-1.0,
			std::string arg2_name="", float arg2_value=-1.0,
			std::string arg3_name="", float arg3_value=-1.0,
			std::string arg4_name="", float arg4_value=-1.0,
			std::string arg5_name="", float arg5_value=-1.0,
			std::string arg6_name="", float arg6_value=-1.0
		);

		/// Close logger (sockets and/or files)
		void Close();

		/// Set or change connection info for logger (i.e. tcp://*:5556)
		void Connection(std::string new_connection);

		/// Enable/Disable logging
		void Enable(bool is_enabled) { enabled = is_enabled;};

		/// Set or change the file path (optional)
		void Path(std::string new_path);

		/// Log message to all subscribers of this logger (if any)
		void Log(std::string message);

		/// Log message to a file (if path set)
		void LogToFile(std::string message);
	};

}

#endif
