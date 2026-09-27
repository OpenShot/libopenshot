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
#include "Logger.h"

#include "Frame.h"
#include "RendererBase.h"
#include "Logger.h"
#include <utility>

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
	std::lock_guard<std::mutex> lock(frame_mutex);
	return frame ? frame->number : 0;
    }

    void VideoPlaybackThread::SetFrame(std::shared_ptr<Frame> next_frame)
    {
	std::lock_guard<std::mutex> lock(frame_mutex);
	frame = std::move(next_frame);
    }

    // Start the thread
    void VideoPlaybackThread::run()
    {
	while (!threadShouldExit()) {
	    // Make other threads wait on the render event
		bool need_render = render.wait(500);
		std::shared_ptr<Frame> frame_to_render;
		if (need_render) {
			std::lock_guard<std::mutex> lock(frame_mutex);
			frame_to_render = frame;
		}

		if (frame_to_render)
		{
			// Debug
			Logger::Instance()->AppendDebugMethod(
				"VideoPlaybackThread::run (before render)",
				"frame->number", frame_to_render->number,
				"need_render", need_render);

			// Render the frame to the screen
			renderer->paint(frame_to_render);
		}

		// Signal to other threads that the rendered event has completed
		rendered.signal();
	}

	return;
    }
}
