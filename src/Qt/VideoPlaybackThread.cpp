/**
 * @file
 * @brief Source file for VideoPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VideoPlaybackThread.h"
#include "ZmqLogger.h"

#include "Frame.h"
#include "RendererBase.h"
#include "ZmqLogger.h"

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
    int64_t VideoPlaybackThread::getCurrentFramePosition()
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
		bool need_render = render.wait(500);

		if (need_render && frame)
		{
			// Debug
			ZmqLogger::Instance()->AppendDebugMethod(
				"VideoPlaybackThread::run (before render)",
				"frame->number", frame->number,
				"need_render", need_render);

			// Render the frame to the screen
			renderer->paint(frame);
		}

		// Signal to other threads that the rendered event has completed
		rendered.signal();
	}

	return;
    }
}
