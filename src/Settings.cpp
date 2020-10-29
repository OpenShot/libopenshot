/**
 * @file
 * @brief Source file for global Settings class
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

#include "Settings.h"

using namespace std;
using namespace openshot;


// Global reference to Settings
Settings *Settings::m_pInstance = NULL;

// Create or Get an instance of the settings singleton
Settings *Settings::Instance()
{
	if (!m_pInstance) {
		// Create the actual instance of Settings only once
		m_pInstance = new Settings;
		m_pInstance->HARDWARE_DECODER = 0;
		m_pInstance->HIGH_QUALITY_SCALING = false;
		m_pInstance->WAIT_FOR_VIDEO_PROCESSING_TASK = false;
		m_pInstance->OMP_THREADS = 12;
		m_pInstance->FF_THREADS = 8;
		m_pInstance->DE_LIMIT_HEIGHT_MAX = 1100;
		m_pInstance->DE_LIMIT_WIDTH_MAX = 1950;
		m_pInstance->HW_DE_DEVICE_SET = 0;
		m_pInstance->HW_EN_DEVICE_SET = 0;
		m_pInstance->PLAYBACK_AUDIO_DEVICE_NAME = "";
		m_pInstance->DEBUG_TO_STDERR = false;
	}

	return m_pInstance;
}
