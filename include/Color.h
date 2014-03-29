/**
 * @file
 * @brief Header file for Color class
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_COLOR_H
#define OPENSHOT_COLOR_H

#include "KeyFrame.h"

namespace openshot {

	/**
	 * @brief This struct represents a color (used on the timeline and clips)
	 *
	 * Colors are represented by 4 curves, representing red, green, and blue.  The curves
	 * can be used to animate colors over time.
	 */
	struct Color{
		Keyframe red; ///<Curve representing the red value (0 - 65536)
		Keyframe green; ///<Curve representing the green value (0 - 65536)
		Keyframe blue; ///<Curve representing the red blue (0 - 65536)

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		void SetJsonValue(Json::Value root); ///< Load Json::JsonValue into this object
	};


}

#endif
