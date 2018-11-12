/**
 * @file
 * @brief Header file for OpenMPUtilities (set some common macros)
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

#ifndef OPENSHOT_OPENMP_UTILITIES_H
#define OPENSHOT_OPENMP_UTILITIES_H

#include <omp.h>
#include <stdlib.h>
#include <string.h>

// Calculate the # of OpenMP and FFmpeg Threads to allow.
#define OPEN_MP_NUM_PROCESSORS omp_get_num_procs()
#define FF_NUM_PROCESSORS omp_get_num_procs()

using namespace std;

namespace openshot {

	// Check if OS2_OMP_THREADS environment variable is present, and return
	// if multiple threads should be used with OMP
	static bool IsOMPEnabled() {
		char* OS2_OMP_THREADS = getenv("OS2_OMP_THREADS");
		if (OS2_OMP_THREADS != NULL && strcmp(OS2_OMP_THREADS, "0") == 0)
			return false;
		else
			return true;
	}

}

#endif
