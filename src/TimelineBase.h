/**
 * @file
 * @brief Header file for Timeline class
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

#ifndef OPENSHOT_TIMELINE_BASE_H
#define OPENSHOT_TIMELINE_BASE_H


namespace openshot {
	/**
	 * @brief This class represents a timeline (used for building generic timeline implementations)
	 */
	class TimelineBase {

	public:
		int preview_width; ///< Optional preview width of timeline image. If your preview window is smaller than the timeline, it's recommended to set this.
		int preview_height; ///< Optional preview width of timeline image. If your preview window is smaller than the timeline, it's recommended to set this.

		/// Constructor for the base timeline
		TimelineBase();
	};
}

#endif
