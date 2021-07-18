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

#include <cstdint>
#include <list>

#include "ReaderBase.h"

namespace openshot {
    // Forward decl
    class Clip;

    /**
     * @brief Contextual data for the current Timeline clip instance
     *
     * When the Timeline requests an openshot::Frame instance from a Clip,
     * it passes details related to frame composition in this struct.
     * Currently the only information it carries relates to stacking
     * position when compositing layers.
     *
     * This info can help determine if a Clip should apply global effects
     * from the Timeline, such as a global Transition/Mask effect.
     */
    struct TimelineInfoStruct
    {
        bool is_top_clip; ///< Is clip on top (if overlapping another clip)
    };

    /**
     * @brief Abstract base class representing a timeline
     */
    class TimelineBase : public openshot::ReaderBase {

    public:
        /// Optional preview width of timeline image.
        ///
        /// If your preview window is smaller than the timeline,
        /// it's recommended to set this.
        int preview_width;

        /// Optional preview width of timeline image.
        ///
        /// If your preview window is smaller than the timeline,
        /// it's recommended to set this.
        int preview_height;

        /// Destructor
        virtual ~TimelineBase() = default;

        // Retrieve a list of clips on the Timeline
        //
        // This is an interface method that must be overridden
        // by subclasses, so that a clips list will be accessible
        // Through the Clips() method of a TimelineBase* pointer.
        virtual std::list<openshot::Clip*> Clips() = 0;

    protected:
        /// Default constructor for the base timeline
        TimelineBase() : ReaderBase::ReaderBase(),
            preview_width(1920), preview_height(1080) {}

    };
}

#endif
