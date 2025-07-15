/**
 * @file
 * @brief Header file for OpenMPUtilities (set some common macros)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_OPENMP_UTILITIES_H
#define OPENSHOT_OPENMP_UTILITIES_H

#include <omp.h>
#include <algorithm>
#include <string>

#include "Settings.h"

// Calculate the # of OpenMP Threads to allow
#define OPEN_MP_NUM_PROCESSORS std::min(omp_get_num_procs(), std::max(2, openshot::Settings::Instance()->OMP_THREADS))
#define FF_VIDEO_NUM_PROCESSORS std::min(omp_get_num_procs(), std::max(2, openshot::Settings::Instance()->FF_THREADS))
#define FF_AUDIO_NUM_PROCESSORS std::min(omp_get_num_procs(), std::max(2, openshot::Settings::Instance()->FF_THREADS))

// Set max-active-levels to the max supported, if possible
// (supported_active_levels is OpenMP 5.0 (November 2018) or later, only.)
#if (_OPENMP >= 201811)
  #define OPEN_MP_MAX_ACTIVE omp_get_supported_active_levels()
#else
  #define OPEN_MP_MAX_ACTIVE OPEN_MP_NUM_PROCESSORS
#endif

#endif
