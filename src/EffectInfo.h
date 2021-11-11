/**
 * @file
 * @brief Header file for the EffectInfo class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_EFFECT_INFO_H
#define OPENSHOT_EFFECT_INFO_H

#include "Json.h"

namespace openshot
{
	class Clip;
	class EffectBase;
	/**
	 * @brief This class returns a listing of all effects supported by libopenshot
	 *
	 * Use this class to return a listing of all supported effects, and their
	 * descriptions.
	 */
	class EffectInfo
	{
	public:
		/// Create an instance of an effect (factory style)
		EffectBase* CreateEffect(std::string effect_type);

		// JSON methods
		static std::string Json(); ///< Generate JSON string of this object
		static Json::Value JsonValue(); ///< Generate Json::Value for this object

	};

}

#endif
