/**
 * @file
 * @brief Source file for ZeroMQ-based Logger class
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

#include "../include/ZmqLogger.h"

#if USE_RESVG == 1
	#include "ResvgQt.h"
#endif

using namespace std;
using namespace openshot;
#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::duration::microseconds


// Global reference to logger
ZmqLogger *ZmqLogger::m_pInstance = NULL;

// Create or Get an instance of the logger singleton
ZmqLogger *ZmqLogger::Instance()
{
	if (!m_pInstance) {
		// Create the actual instance of logger only once
		m_pInstance = new ZmqLogger;

		// init ZMQ variables
		m_pInstance->context = NULL;
		m_pInstance->publisher = NULL;
		m_pInstance->connection = "";

		// Default connection
		m_pInstance->Connection("tcp://*:5556");

		// Init enabled to False (force user to call Enable())
		m_pInstance->enabled = false;

		#if USE_RESVG == 1
			// Init resvg logging (if needed)
			// This can only happen 1 time or it will crash
			ResvgRenderer::initLog();
		#endif

	}

	return m_pInstance;
}

// Set the connection for this logger
void ZmqLogger::Connection(std::string new_connection)
{
	// Create a scoped lock, allowing only a single thread to run the following code at one time
	const GenericScopedLock<CriticalSection> lock(loggerCriticalSection);

	// Does anything need to happen?
	if (new_connection == connection)
		return;
	else
		// Set new connection
		connection = new_connection;

	if (context == NULL) {
		// Create ZMQ Context
		context = new zmq::context_t(1);
	}

	if (publisher != NULL) {
		// Close an existing bound publisher socket
		publisher->close();
		publisher = NULL;
	}

	// Create new publisher instance
	publisher = new zmq::socket_t(*context, ZMQ_PUB);

	// Bind to the socket
	try {
		publisher->bind(connection.c_str());

	} catch (zmq::error_t &e) {
		std::cout << "ZmqLogger::Connection - Error binding to " << connection << ". Switching to an available port." << std::endl;
		connection = "tcp://*:*";
		publisher->bind(connection.c_str());
	}

	// Sleeping to allow connection to wake up (0.25 seconds)
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void ZmqLogger::Log(std::string message)
{
	if (!enabled)
		// Don't do anything
		return;

	// Create a scoped lock, allowing only a single thread to run the following code at one time
	const GenericScopedLock<CriticalSection> lock(loggerCriticalSection);

	// Send message over socket (ZeroMQ)
	zmq::message_t reply (message.length());
	std::memcpy (reply.data(), message.c_str(), message.length());

#if ZMQ_VERSION > ZMQ_MAKE_VERSION(4, 3, 1)
	// Set flags for immediate delivery (new API)
	publisher->send(reply, zmq::send_flags::dontwait);
#else
	publisher->send(reply);
#endif

	// Also log to file, if open
	LogToFile(message);
}

// Log message to a file (if path set)
void ZmqLogger::LogToFile(std::string message)
{
	// Write to log file (if opened, and force it to write to disk in case of a crash)
	if (log_file.is_open())
		log_file << message << std::flush;
}

void ZmqLogger::Path(std::string new_path)
{
	// Update path
	file_path = new_path;

	// Close file (if already open)
	if (log_file.is_open())
		log_file.close();

	// Open file (write + append)
	log_file.open (file_path.c_str(), std::ios::out | std::ios::app);

	// Get current time and log first message
	std::time_t now = std::time(0);
	std::tm* localtm = std::localtime(&now);
	log_file << "------------------------------------------" << std::endl;
	log_file << "libopenshot logging: " << std::asctime(localtm);
	log_file << "------------------------------------------" << std::endl;
}

void ZmqLogger::Close()
{
	// Disable logger as it no longer needed
	enabled = false;

	// Close file (if already open)
	if (log_file.is_open())
		log_file.close();

	// Close socket (if any)
	if (publisher != NULL) {
		// Close an existing bound publisher socket
		publisher->close();
		publisher = NULL;
	}
}

// Append debug information
void ZmqLogger::AppendDebugMethod(std::string method_name,
				  std::string arg1_name, float arg1_value,
				  std::string arg2_name, float arg2_value,
				  std::string arg3_name, float arg3_value,
				  std::string arg4_name, float arg4_value,
				  std::string arg5_name, float arg5_value,
				  std::string arg6_name, float arg6_value)
{
	if (!enabled)
		// Don't do anything
		return;

	{
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(loggerCriticalSection);

		std::stringstream message;
		message << std::fixed << std::setprecision(4);
		message << method_name << " (";

		// Add attributes to method JSON
		if (arg1_name.length() > 0)
			message << arg1_name << "=" << arg1_value;

		if (arg2_name.length() > 0)
			message << ", " << arg2_name << "=" << arg2_value;

		if (arg3_name.length() > 0)
			message << ", " << arg3_name << "=" << arg3_value;

		if (arg4_name.length() > 0)
			message << ", " << arg4_name << "=" << arg4_value;

		if (arg5_name.length() > 0)
			message << ", " << arg5_name << "=" << arg5_value;

		if (arg6_name.length() > 0)
			message << ", " << arg6_name << "=" << arg6_value;

		// Output to standard output
		message << ")" << endl;

		// Send message through ZMQ
		Log(message.str());
	}
}
