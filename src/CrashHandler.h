/**
 * @file
 * @brief Header file for CrashHandler class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

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
		CrashHandler(){return;}; 						 // Don't allow user to create an instance of this singleton

		/// Default copy method
		//CrashHandler(CrashHandler const&){};             // Don't allow the user to copy this instance
		CrashHandler(CrashHandler const&) = delete;             // Don't allow the user to copy this instance

		/// Default assignment operator
		CrashHandler & operator=(CrashHandler const&) = delete;  // Don't allow the user to assign this instance

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
