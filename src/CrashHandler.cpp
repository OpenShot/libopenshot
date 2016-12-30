/**
 * @file
 * @brief Source file for CrashHandler class
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

#include "../include/CrashHandler.h"

using namespace std;
using namespace openshot;


// Global reference to logger
CrashHandler *CrashHandler::m_pInstance = NULL;

// Create or Get an instance of the logger singleton
CrashHandler *CrashHandler::Instance()
{
	if (!m_pInstance) {
		// Create the actual instance of crash handler only once
		m_pInstance = new CrashHandler;

#ifdef __MINGW32__
		// TODO: Windows exception handling methods
		signal(SIGSEGV, CrashHandler::abortHandler);

#else
		struct sigaction sa;
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = CrashHandler::abortHandler;
		sigemptyset( &sa.sa_mask );

		// Register abortHandler function callback
		sigaction( SIGABRT, &sa, NULL );
		sigaction( SIGSEGV, &sa, NULL );
		sigaction( SIGBUS,  &sa, NULL );
		sigaction( SIGILL,  &sa, NULL );
		sigaction( SIGFPE,  &sa, NULL );
		sigaction( SIGPIPE, &sa, NULL );
#endif
	}

	return m_pInstance;
}

#ifdef __MINGW32__
// Windows exception handler
void CrashHandler::abortHandler(int signum)
{
	// Associate each signal with a signal name string.
	const char* name = NULL;
	switch( signum )
	{
		case SIGABRT: name = "SIGABRT";  break;
		case SIGSEGV: name = "SIGSEGV";  break;
		case SIGILL:  name = "SIGILL";   break;
		case SIGFPE:  name = "SIGFPE";   break;
	}

	// Notify the user which signal was caught
	if ( name )
		fprintf( stderr, "Caught signal %d (%s)\n", signum, name );
	else
		fprintf( stderr, "Caught signal %d\n", signum );

	// Dump a stack trace.
	printStackTrace(stderr, 63);

	// Quit
	exit( signum );
}
#else
// Linux and Mac Exception Handler
void CrashHandler::abortHandler( int signum, siginfo_t* si, void* unused )
{
	// Associate each signal with a signal name string.
	const char* name = NULL;
	switch( signum )
	{
		case SIGABRT: name = "SIGABRT";  break;
		case SIGSEGV: name = "SIGSEGV";  break;
		case SIGBUS:  name = "SIGBUS";   break;
		case SIGILL:  name = "SIGILL";   break;
		case SIGFPE:  name = "SIGFPE";   break;
		case SIGPIPE:  name = "SIGPIPE";   break;
	}

	// Notify the user which signal was caught
	if ( name )
		fprintf( stderr, "Caught signal %d (%s)\n", signum, name );
	else
		fprintf( stderr, "Caught signal %d\n", signum );

	// Dump a stack trace.
	printStackTrace(stderr, 63);

	// Quit
	exit( signum );
}
#endif

void CrashHandler::printStackTrace(FILE *out, unsigned int max_frames)
{
	fprintf(out, "---- Unhandled Exception: Stack Trace ----\n");
	ZmqLogger::Instance()->LogToFile("---- Unhandled Exception: Stack Trace ----\n");
	stringstream stack_output;

#ifdef __MINGW32__
	// Windows stack unwinding
	HANDLE process = GetCurrentProcess();
	HANDLE thread = GetCurrentThread();

	CONTEXT context;
	memset(&context, 0, sizeof(CONTEXT));
	context.ContextFlags = CONTEXT_FULL;
	RtlCaptureContext(&context);

	SymInitialize(process, NULL, TRUE);

	DWORD image;
	STACKFRAME64 stackframe;
	ZeroMemory(&stackframe, sizeof(STACKFRAME64));

#ifdef _M_IX86
	image = IMAGE_FILE_MACHINE_I386;
	stackframe.AddrPC.Offset = context.Eip;
	stackframe.AddrPC.Mode = AddrModeFlat;
	stackframe.AddrFrame.Offset = context.Ebp;
	stackframe.AddrFrame.Mode = AddrModeFlat;
	stackframe.AddrStack.Offset = context.Esp;
	stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    image = IMAGE_FILE_MACHINE_AMD64;
	stackframe.AddrPC.Offset = context.Rip;
	stackframe.AddrPC.Mode = AddrModeFlat;
	stackframe.AddrFrame.Offset = context.Rsp;
	stackframe.AddrFrame.Mode = AddrModeFlat;
	stackframe.AddrStack.Offset = context.Rsp;
	stackframe.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
	image = IMAGE_FILE_MACHINE_IA64;
	stackframe.AddrPC.Offset = context.StIIP;
	stackframe.AddrPC.Mode = AddrModeFlat;
	stackframe.AddrFrame.Offset = context.IntSp;
	stackframe.AddrFrame.Mode = AddrModeFlat;
	stackframe.AddrBStore.Offset = context.RsBSP;
	stackframe.AddrBStore.Mode = AddrModeFlat;
	stackframe.AddrStack.Offset = context.IntSp;
	stackframe.AddrStack.Mode = AddrModeFlat;
#endif

	// Loop through the entire stack
	for (size_t i = 0; i < max_frames; i++) {

		BOOL result = StackWalk64(
				image, process, thread,
				&stackframe, &context, NULL,
				SymFunctionTableAccess64, SymGetModuleBase64, NULL);

		if (i <= 2) { continue; } // Skip the first 3 elements (those relate to these functions)
		if (!result) { break; }

		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;
		WINBOOL found_symbol = SymFromAddr(process, stackframe.AddrPC.Offset, NULL, symbol);

		if (found_symbol) {
			printf("[%i] %s, address 0x%0X\n", i, symbol->Name, symbol->Address);
			stack_output << left << setw(30) << symbol->Name << " " << setw(40) << std::hex << symbol->Address << std::dec << endl;
		} else {
			printf("[%i] ???\n", i);
			stack_output << left << setw(30) << "???" << endl;
		}
	}
	SymCleanup(process);

#else
    // Linux and Mac stack unwinding
	// Storage array for stack trace address data
	void* addrlist[max_frames+1];

	// Retrieve current stack addresses
	unsigned int addrlen = backtrace( addrlist, sizeof( addrlist ) / sizeof( void* ));

	if ( addrlen == 0 )
	{
		fprintf(out, "  No stack trace found (addrlen == 0)\n");
		ZmqLogger::Instance()->LogToFile("  No stack trace found (addrlen == 0)\n");
		return;
	}

	// Resolve addresses into strings containing "filename(function+address)",
	// Actually it will be ## program address function + offset
	// this array must be free()-ed
	char** symbollist = backtrace_symbols( addrlist, addrlen );

	size_t funcnamesize = 1024;
	char funcname[1024];

	// Iterate over the returned symbol lines. Skip the first 4, it is the
	// address of this function.
	for ( unsigned int i = 4; i < addrlen; i++ )
	{
		char* begin_name   = NULL;
		char* begin_offset = NULL;
		char* end_offset   = NULL;

		// Find parentheses and +address offset surrounding the mangled name
#ifdef DARWIN
		// OSX style stack trace
      for ( char *p = symbollist[i]; *p; ++p )
      {
         if (( *p == '_' ) && ( *(p-1) == ' ' ))
            begin_name = p-1;
         else if ( *p == '+' )
            begin_offset = p-1;
      }

      if ( begin_name && begin_offset && ( begin_name < begin_offset ))
      {
         *begin_name++ = '\0';
         *begin_offset++ = '\0';

         // Mangled name is now in [begin_name, begin_offset) and caller
         // offset in [begin_offset, end_offset). now apply
         // __cxa_demangle():
         int status;
         char* ret = abi::__cxa_demangle( begin_name, &funcname[0], &funcnamesize, &status );
         if ( status == 0 )
         {
            funcname = ret; // Use possibly realloc()-ed string
            fprintf( out, "  %-30s %-40s %s\n", symbollist[i], funcname, begin_offset );
            stack_output << left << " " << setw(30) << symbollist[i] << " " << setw(40) << funcname << " " << begin_offset << endl;
         } else {
            // Demangling failed. Output function name as a C function with
            // no arguments.
            fprintf( out, "  %-30s %-38s() %s\n", symbollist[i], begin_name, begin_offset );
            stack_output << left << "  " << setw(30) << symbollist[i] << " " << setw(38) << begin_name << " " << begin_offset << endl;
         }

#else // !DARWIN - but is posix
		// not OSX style
		// ./module(function+0x15c) [0x8048a6d]
		for ( char *p = symbollist[i]; *p; ++p )
		{
			if ( *p == '(' )
				begin_name = p;
			else if ( *p == '+' )
				begin_offset = p;
			else if ( *p == ')' && ( begin_offset || begin_name ))
				end_offset = p;
		}

		if ( begin_name && end_offset && ( begin_name < end_offset ))
		{
			*begin_name++   = '\0';
			*end_offset++   = '\0';
			if ( begin_offset )
				*begin_offset++ = '\0';

			// Mangled name is now in [begin_name, begin_offset) and caller
			// offset in [begin_offset, end_offset). now apply
			// __cxa_demangle():
			int status = 0;
			char* ret = abi::__cxa_demangle( begin_name, funcname, &funcnamesize, &status );
			char* fname = begin_name;
			if ( status == 0 )
				fname = ret;

			if ( begin_offset )
			{
				fprintf( out, "  %-30s ( %-40s  + %-6s) %s\n", symbollist[i], fname, begin_offset, end_offset );
				stack_output << left << "  " << setw(30) << symbollist[i] << " " << setw(40) << fname << " " << begin_offset << " " << end_offset << endl;

			} else {
				fprintf( out, "  %-30s ( %-40s    %-6s) %s\n", symbollist[i], fname, "", end_offset );
				stack_output << left << "  " << setw(30) << symbollist[i] << " " << setw(40) << fname << " " << end_offset << endl;

			}
#endif  // !DARWIN - but is posix
		} else {
			// Couldn't parse the line? print the whole line.
			fprintf(out, "  %-40s\n", symbollist[i]);
			stack_output << left << "  " << setw(40) << symbollist[i] << endl;
		}
	}

	// Free array
	free(symbollist);
#endif

	// Write stacktrace to file (if log path set)
	ZmqLogger::Instance()->LogToFile(stack_output.str());

	fprintf(out, "---- End of Stack Trace ----\n");
	ZmqLogger::Instance()->LogToFile("---- End of Stack Trace ----\n");
}