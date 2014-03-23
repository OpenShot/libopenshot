/**
 * @file
 * @brief Source file for VideoPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "../include/ReaderBase.h"
#include "../include/RendererBase.h"
#include "VideoPlaybackThread.h"

namespace openshot
{
	// Constructor
    VideoPlaybackThread::VideoPlaybackThread(RendererBase *rb)
	: Thread("video-playback"), renderer(rb)
	, render(), reset(false)
    {
    }

    // Destructor
    VideoPlaybackThread::~VideoPlaybackThread()
    {
    }

    // Get the currently playing frame number (if any)
    int VideoPlaybackThread::getCurrentFramePosition()
    {
    	if (frame)
    		return frame->number;
    	else
    		return 0;
    }

    // Start the thread
    void VideoPlaybackThread::run()
    {
	while (!threadShouldExit()) {
	    // Make other threads wait on the render event
		render.wait();
		// Render the frame to the screen
	    renderer->paint(frame);
	    // Signal to other threads that the rendered event has completed
	    rendered.signal();
	}
    }
}
