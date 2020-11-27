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

#include "ZmqLogger.h"
#include "Settings.h"

#if USE_RESVG == 1
	#include "ResvgQt.h"
#endif

#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::duration::microseconds

using namespace openshot;

// Global reference to logger
ZmqLogger *ZmqLogger::m_pInstance = nullptr;

// Create or Get an instance of the logger singleton
ZmqLogger *ZmqLogger::Instance()
{
	if (!m_pInstance) {
		// Create the actual instance of logger only once
		m_pInstance = new ZmqLogger;

		// init ZMQ variables
		m_pInstance->m_context.reset();
		m_pInstance->m_publisher.reset();
		m_pInstance->m_connection = "";

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
void ZmqLogger::Connection(const std::string& new_connection)
{
	using Context = std::unique_ptr<zmq::context_t>;
	using Publisher = std::unique_ptr<zmq::socket_t>;

	// Execute this block in only one thread at a time
	const std::lock_guard<std::recursive_mutex> lock(mutex);

	// Does anything need to happen?
	if (new_connection == m_connection)
		return;
	else
		// Set new connection
		m_connection = new_connection;

	if (!m_context) {
		// Create ZMQ Context
		m_context = Context(new zmq::context_t(1));
	}

	if (m_publisher) {
		// Close an existing bound publisher socket
		m_publisher->close();
		m_publisher.reset();
	}

	// Create new publisher instance
	m_publisher = Publisher(new zmq::socket_t(*m_context, ZMQ_PUB));

	// Bind to the socket
	try {
		m_publisher->bind(m_connection.c_str());

	} catch (zmq::error_t &e) {
		std::cout << "ZmqLogger::Connection - Error binding to "
		          << m_connection << ". Switching to an available port.\n";
		m_connection = "tcp://*:*";
		m_publisher->bind(m_connection.c_str());
	}

	// Sleeping to allow connection to wake up (0.25 seconds)
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void ZmqLogger::SendLog(const std::string& message)
{
	if (!enabled)
		return;

	// Execute this block in only one thread at a time
	const std::lock_guard<std::recursive_mutex> lock(mutex);

#if ZMQ_VERSION > ZMQ_MAKE_VERSION(4, 3, 1)
	// Send with flags for immediate delivery (new API)
	m_publisher->send(zmq::buffer(message), zmq::send_flags::dontwait);
#else
	// Construct message and send (clunky old API)
	zmq::message_t zmq_msg(message.length());
	std::memcpy(zmq_msg.data(), message.c_str(), message.length());
	m_publisher->send(zmq_msg);
#endif
}

// Log message to a file (if path set)
void ZmqLogger::LogToFile(const std::string& message)
{
	if (enabled && m_logFile.is_open())
		// Write to log file (and flush in case of a crash)
		m_logFile << message << std::endl;
}

void ZmqLogger::LogToStderr(const std::string& message) {
	if (GetSettings()->DEBUG_TO_STDERR) {
		// Print message to stderr (using thread-safe stream)
		std::clog << message << std::endl;
	}
}

void ZmqLogger::Log(const std::string& message) {
	LogToStderr(message);
	// Send message through ZMQ
	SendLog(message);
	LogToFile(message);
}

void ZmqLogger::Path(const std::string& new_path)
{
	// Execute this block in only one thread at a time
	const std::lock_guard<std::recursive_mutex> lock(mutex);

	// Close file (if already open)
	if (m_logFile.is_open())
		m_logFile.close();

	// Update path
	m_filePath = new_path;

	if (m_filePath == "")
		return;

	// Open file (write + append)
	m_logFile.open(m_filePath.c_str(), std::ios::out | std::ios::app);

	// Get current time and log first message
	std::time_t now = std::time(0);
	std::tm* localtm = std::localtime(&now);
	m_logFile << "------------------------------------------\n";
	m_logFile << "libopenshot logging: " << std::asctime(localtm);
	m_logFile << "------------------------------------------\n";
}

void ZmqLogger::Close()
{
	// Disable logger as it no longer needed
	enabled = false;

	// Close file (if already open)
	if (m_logFile.is_open())
		m_logFile.close();

	// Close socket (if any)
	if (m_publisher) {
		// Close an existing bound publisher socket
		m_publisher->close();
		m_publisher.reset();
	}
}

// Append debug information
void ZmqLogger::AppendDebugMethod(
	std::string method_name,
	std::string arg1_name, float arg1_value,
	std::string arg2_name, float arg2_value,
	std::string arg3_name, float arg3_value,
	std::string arg4_name, float arg4_value,
	std::string arg5_name, float arg5_value,
	std::string arg6_name, float arg6_value)
{
	if (!enabled && !GetSettings()->DEBUG_TO_STDERR)
		// Don't do anything
		return;

	std::stringstream message;
	message << std::fixed << std::setprecision(4);

	// Construct message
	message << method_name << " (";

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

	message << ")";
	Log(message.str());
}
