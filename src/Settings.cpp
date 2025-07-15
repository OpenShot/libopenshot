/**
 * @file
 * @brief Source file for global Settings class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cstdlib>
#include <omp.h>
#include "Settings.h"

using namespace openshot;

// Global reference to Settings
Settings *Settings::m_pInstance = nullptr;

// Create or Get an instance of the settings singleton
Settings *Settings::Instance()
{
	if (!m_pInstance) {
		// Create the actual instance of Settings only once
		m_pInstance = new Settings;
		m_pInstance->OMP_THREADS = omp_get_num_procs();
		m_pInstance->FF_THREADS = omp_get_num_procs();
		auto env_debug = std::getenv("LIBOPENSHOT_DEBUG");
		if (env_debug != nullptr)
			m_pInstance->DEBUG_TO_STDERR = true;
	}

	return m_pInstance;
}
