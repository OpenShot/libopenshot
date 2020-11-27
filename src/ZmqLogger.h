/**
 * @file
 * @brief Header file for ZeroMQ-based Logger class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#include <string>
#include <mutex>
#include <fstream>
#include <sstream>
#include <functional>
#include <memory>
#include <zmq.hpp>

#ifndef zmqLog
	#define zmqLog() \
		openshot::StreamLog(openshot::StreamLog::zmqLogFunction).GetStream()
#endif
#ifndef LOGVAR
	#define LOGVAR(VAR) #VAR << " = " << VAR
#endif

#ifndef DebugLog
	#define DebugLog(args...) openshot::ZmqLogger::Instance()->AppendDebugMethod(args)
#endif
#ifndef DEBUGVAR
	#define DEBUGVAR(VAR) #VAR, VAR
#endif

namespace openshot {
	/**
	 * @brief This class is used for logging and sending those logs over a ZemoMQ socket to a listener
	 *
	 * OpenShot desktop editor listens to this port, to receive libopenshot debug output. It both logs to
	 * a file and sends the stdout over a socket.
	 */
	class ZmqLogger {
		/// Type aliases
		using Context = std::unique_ptr<zmq::context_t>;
		using Publisher = std::unique_ptr<zmq::socket_t>;

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

		/// Output message using all available logging methods
		void Log(const std::string& message);

		/// Log message to all messagebus subscribers
		void SendLog(const std::string& message);

		/// Log message to a file (if path set)
		void LogToFile(const std::string& message);

		/// Log message to console error stream (stderr)
		void LogToStderr(const std::string& message);

		/// Enable/Disable logging
		void Enable(bool is_enabled) { enabled = is_enabled;};

		/// Close logger (sockets and/or files)
		void Close();

		/// Set or change connection info for logger (i.e. tcp://*:5556)
		void Connection(const std::string& new_connection);

		/// Set or change the file path (optional)
		void Path(const std::string& new_path);

	private:
		std::recursive_mutex mutex;
		std::string m_connection;

		// Logfile related vars
		std::string m_filePath;
		std::ofstream m_logFile;
		bool enabled;

		/// ZMQ Context
		Context m_context;

		/// ZMQ Socket
		Publisher m_publisher;

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
	};

    /**
     * @brief Stream-based logging class which feeds to ZmqLogger
     *
     * ZmqLogger.h includes two convenience macros intended for use
     * with this class. They make logging quick and painless, especially when
     * logging variables and their value.
     *
     * @code
     * // Use the zmqLog() macro to create an instance of StreamLogger
     * zmqLog() << "Hyperframulated the flux capacitor!";
     *
     * // To log a variable with its value, use the LOGVAR() macro
     * int x = 5;
     * int y = 10;
     * zmqLog() << "Out of range! " << LOGVAR(x) << ", " << LOGVAR(y);
     * @endcode
     *
     * These messages will be logged:
     * 1: Hyperframulated the flux capacitor!
     * 2: Out of range! x = 5, y = 10
	 *
    **/

	// Implementation largely inspired by this StackOverflow answer:
	//   https://stackoverflow.com/a/48475646/200794
	// Referencing the Dr. Dobbs article "Logging In C++" by Petru Marginean
	//   https://www.drdobbs.com/cpp/logging-in-c/201804215
    class StreamLog {
        using LogFunction = std::function<void(const std::string&)>;

    public:
		/// Construct a StreamLog instance that calls logFunction to output messages
        explicit StreamLog(LogFunction logFunction) : m_logFunction(std::move(logFunction)) {};
		/// Return the logging stream that outputs to the selected function
        std::ostringstream& GetStream() { return m_stringStream; }
		/// Destroy the logging instance, which calls logFunction to log the stream
        ~StreamLog() { m_logFunction(m_stringStream.str()); }
		/// A logging function that delivers messages to ZmqLogger
		static void zmqLogFunction(const std::string& message) {
			ZmqLogger::Instance()->Log(message);
		}
    private:
        std::ostringstream m_stringStream;
        LogFunction m_logFunction;
    };
}

#endif
