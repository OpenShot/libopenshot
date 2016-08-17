/**
 * @file
 * @brief Header file for the EffectInfo class
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

#ifndef OPENSHOT_EFFECT_INFO_H
#define OPENSHOT_EFFECT_INFO_H

#include "Effects.h"


using namespace std;

namespace openshot
{

	/**
	 * @brief This class returns a listing of all effects supported by libopenshot
	 *
	 * Use this class to return a listing of all supported effects, and their
	 * descriptions.
	 */
	class EffectInfo
	{
	public:
		// Create an instance of an effect (factory style)
		EffectBase* CreateEffect(string effect_type);

		/// JSON methods
		static string Json(); ///< Generate JSON string of this object
		static Json::Value JsonValue(); ///< Generate Json::JsonValue for this object

	};

}

#endif
