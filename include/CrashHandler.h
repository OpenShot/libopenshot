/**
 * @file
 * @brief Header file for CrashHandler class
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

#ifndef OPENSHOT_CRASH_HANDLER_H
#define OPENSHOT_CRASH_HANDLER_H

#include <cstdlib>
#include <stdio.h>
#include <signal.h>
#ifdef __MINGW32__
	#include <winsock2.h>
	#include <windows.h>
	#include <DbgHelp.h>
#else
	#include <execinfo.h>
#endif
#include <errno.h>
#include <cxxabi.h>
#include "ZmqLogger.h"

namespace openshot {

	/**
	 * @brief This class is designed to catch exceptions thrown by libc (SIGABRT, SIGSEGV, SIGILL, SIGFPE)
	 *
	 * This class is a singleton which only needs to be instantiated 1 time, and it will register as a signal
	 * handler with libc, and log errors using the ZmqLogger class.
	 */
	class CrashHandler {
	private:
		/// Default constructor
		CrashHandler(){}; 						 // Don't allow user to create an instance of this singleton

		/// Default copy method
		CrashHandler(CrashHandler const&){};             // Don't allow the user to copy this instance

		/// Default assignment operator
		CrashHandler & operator=(CrashHandler const&){};  // Don't allow the user to assign this instance

		/// Private variable to keep track of singleton instance
		static CrashHandler *m_pInstance;

	public:
		/// Create or get an instance of this crash handler singleton (invoke the class with this method). This also
		/// registers the instance as a signal handler for libc
		static CrashHandler *Instance();

#ifdef __MINGW32__
		// TODO: Windows exception handling methods
		static void abortHandler(int signum);
#else
		/// Method which handles crashes and logs error
		static void abortHandler(int signum, siginfo_t* si, void* unused);
#endif

		/// Method which prints a stacktrace
		static void printStackTrace(FILE *out, unsigned int max_frames);
	};

}

#endif
