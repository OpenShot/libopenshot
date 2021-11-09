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

#include <cstdlib>        // For std::getenv

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
		m_pInstance->HARDWARE_DECODER = 0;
		m_pInstance->HIGH_QUALITY_SCALING = false;
		m_pInstance->OMP_THREADS = 12;
		m_pInstance->FF_THREADS = 8;
		m_pInstance->DE_LIMIT_HEIGHT_MAX = 1100;
		m_pInstance->DE_LIMIT_WIDTH_MAX = 1950;
		m_pInstance->HW_DE_DEVICE_SET = 0;
		m_pInstance->HW_EN_DEVICE_SET = 0;
		m_pInstance->PLAYBACK_AUDIO_DEVICE_NAME = "";
		m_pInstance->DEBUG_TO_STDERR = false;
		auto env_debug = std::getenv("LIBOPENSHOT_DEBUG");
		if (env_debug != nullptr)
			m_pInstance->DEBUG_TO_STDERR = true;
	}

	return m_pInstance;
}
