# Redistribution and use is allowed according to the terms of the New
# BSD license:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#  - Run Doxygen
#
# Adds a doxygen target that runs doxygen to generate the html
# and optionally the LaTeX API documentation.
# The doxygen target is added to the doc target as a dependency.
# i.e.: the API documentation is built with:
#  make doc
#
# USAGE: GLOBAL INSTALL
#
# Install it with:
#  cmake ./ && sudo make install
# Add the following to the CMakeLists.txt of your project:
#  include(UseDoxygen OPTIONAL)
# Optionally copy Doxyfile.in in the directory of CMakeLists.txt and edit it.
#
# USAGE: INCLUDE IN PROJECT
#
#  set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
#  include(UseDoxygen)
# Add the Doxyfile.in and UseDoxygen.cmake files to the projects source directory.
#
#
# CONFIGURATION
#
# To configure Doxygen you can edit Doxyfile.in and set some variables in cmake.
# Variables you may define are:
#  DOXYFILE_SOURCE_DIR - Path where the Doxygen input files are.
#  	Defaults to the current source directory.
#  DOXYFILE_EXTRA_SOURCES - Additional source diretories/files for Doxygen to scan.
#  	The Paths should be in double quotes and separated by space. e.g.:
#  	 "${CMAKE_CURRENT_BINARY_DIR}/foo.c" "${CMAKE_CURRENT_BINARY_DIR}/bar/"
#  
#  DOXYFILE_OUTPUT_DIR - Path where the Doxygen output is stored.
#  	Defaults to "${CMAKE_CURRENT_BINARY_DIR}/doc".
#  
#  DOXYFILE_LATEX - ON/OFF; Set to "ON" if you want the LaTeX documentation
#  	to be built.
#  DOXYFILE_LATEX_DIR - Directory relative to DOXYFILE_OUTPUT_DIR where
#  	the Doxygen LaTeX output is stored. Defaults to "latex".
#  
#  DOXYFILE_HTML_DIR - Directory relative to DOXYFILE_OUTPUT_DIR where
#  	the Doxygen html output is stored. Defaults to "html".
#

#
#  Copyright (c) 2009, 2010, 2011 Tobias Rautenkranz <tobias@rautenkranz.ch>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#

macro(usedoxygen_set_default name value type docstring)
	if(NOT DEFINED "${name}")
		set("${name}" "${value}" CACHE "${type}" "${docstring}")
	endif()
endmacro()

find_package(Doxygen)

if(DOXYGEN_FOUND)
	find_file(DOXYFILE_IN "Doxyfile.in"
			PATHS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_ROOT}/Modules/"
			NO_DEFAULT_PATH
			DOC "Path to the doxygen configuration template file")
	set(DOXYFILE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(DOXYFILE_IN DEFAULT_MSG "DOXYFILE_IN")
endif()

if(DOXYGEN_FOUND AND DOXYFILE_IN_FOUND)
	usedoxygen_set_default(DOXYFILE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc"
		PATH "Doxygen output directory")
	usedoxygen_set_default(DOXYFILE_HTML_DIR "html"
		STRING "Doxygen HTML output directory")
	usedoxygen_set_default(DOXYFILE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
		PATH "Input files source directory")
	usedoxygen_set_default(DOXYFILE_EXTRA_SOURCE_DIRS ""
		STRING "Additional source files/directories separated by space")
	set(DOXYFILE_SOURE_DIRS "\"${DOXYFILE_SOURCE_DIR}\" ${DOXYFILE_EXTRA_SOURCES}")

	usedoxygen_set_default(DOXYFILE_LATEX OFF BOOL "Generate LaTeX API documentation")
	usedoxygen_set_default(DOXYFILE_LATEX_DIR "latex" STRING "LaTex output directory")

	mark_as_advanced(DOXYFILE_OUTPUT_DIR DOXYFILE_HTML_DIR DOXYFILE_LATEX_DIR
		DOXYFILE_SOURCE_DIR DOXYFILE_EXTRA_SOURCE_DIRS DOXYFILE_IN)

	## Dot
	usedoxygen_set_default(DOXYFILE_USE_DOT ON BOOL "Use dot (part of graphviz) to generate graphs")
	set(DOXYFILE_DOT "NO")
	if(DOXYFILE_USE_DOT AND DOXYGEN_DOT_EXECUTABLE)
		set(DOXYFILE_DOT "YES")
	endif()

	set_property(DIRECTORY 
		APPEND PROPERTY
		ADDITIONAL_MAKE_CLEAN_FILES
		"${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_HTML_DIR}")

	add_custom_target(doxygen
		COMMAND "${DOXYGEN_EXECUTABLE}"
			"${DOXYFILE}" 
		COMMENT "Writing documentation to ${DOXYFILE_OUTPUT_DIR}..."
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

	## LaTeX
	set(DOXYFILE_PDFLATEX "NO")

	set_property(DIRECTORY APPEND PROPERTY
		ADDITIONAL_MAKE_CLEAN_FILES
		"${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_LATEX_DIR}")

	if(DOXYFILE_LATEX)
		set(DOXYFILE_GENERATE_LATEX "YES")
		find_package(LATEX)
		find_program(DOXYFILE_MAKE make)
		mark_as_advanced(DOXYFILE_MAKE)
		if(LATEX_COMPILER AND MAKEINDEX_COMPILER AND DOXYFILE_MAKE)
			if(PDFLATEX_COMPILER)
				set(DOXYFILE_PDFLATEX "YES")
			endif()

			add_custom_command(TARGET doxygen
				POST_BUILD
				COMMAND "${DOXYFILE_MAKE}"
				COMMENT	"Running LaTeX for Doxygen documentation in ${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_LATEX_DIR}..."
				WORKING_DIRECTORY "${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_LATEX_DIR}")
		else()
			set(DOXYGEN_LATEX "NO")
		endif()
	else()
		set(DOXYFILE_GENERATE_LATEX "NO")
	endif()


	configure_file("${DOXYFILE_IN}" "${DOXYFILE}" @ONLY)

	if(TARGET doc)
		get_target_property(DOC_TARGET doc TYPE)
	endif()
	if(NOT DOC_TARGET)
		add_custom_target(doc)
	endif()

	add_dependencies(doc doxygen)
endif()
