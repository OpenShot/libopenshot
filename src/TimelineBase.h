/**
 * @file
 * @brief Header file for Timeline class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_TIMELINE_BASE_H
#define OPENSHOT_TIMELINE_BASE_H

#include <cstdint>
#include <list>


namespace openshot {
	// Forward decl
	class Clip;

	/**
	 * @brief This struct contains info about the current Timeline clip instance
	 *
	 * When the Timeline requests an openshot::Frame instance from a Clip, it passes
	 * this struct along, with some additional details from the Timeline, such as if this clip is
	 * above or below overlapping clips, etc... This info can help determine if a Clip should apply
	 * global effects from the Timeline, such as a global Transition/Mask effect.
	 */
	struct TimelineInfoStruct
	{
		bool is_top_clip;				 ///< Is clip on top (if overlapping another clip)
		bool is_before_clip_keyframes;	///< Is this before clip keyframes are applied
	};

	/**
	 * @brief This class represents a timeline (used for building generic timeline implementations)
	 */
	class TimelineBase {

	public:
		int preview_width; ///< Optional preview width of timeline image. If your preview window is smaller than the timeline, it's recommended to set this.
		int preview_height; ///< Optional preview width of timeline image. If your preview window is smaller than the timeline, it's recommended to set this.

		/// Constructor for the base timeline
		TimelineBase();

		/// This function will be overloaded in the Timeline class passing no arguments
		/// so we'll be able to access the Timeline::Clips() function from a pointer object of
		/// the TimelineBase class
		virtual std::list<openshot::Clip*> Clips() = 0;

		virtual ~TimelineBase() = default;
	};
}

#endif
