/**
 * @file
 * @brief Source file for VideoCacheThread class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VideoCacheThread.h"

#include "CacheBase.h"
#include "Exceptions.h"
#include "Frame.h"
#include "OpenMPUtilities.h"
#include "ZmqLogger.h"

#include <algorithm>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono::milliseconds

namespace openshot
{
	// Constructor
	VideoCacheThread::VideoCacheThread()
	: Thread("video-cache"), speed(1), is_playing(false), position(1)
	, reader(NULL), max_concurrent_frames(OPEN_MP_NUM_PROCESSORS * 4), current_display_frame(1)
    {
    }

    // Destructor
	VideoCacheThread::~VideoCacheThread()
    {
    }

    // Get the currently playing frame number (if any)
    int64_t VideoCacheThread::getCurrentFramePosition()
    {
    	if (frame)
    		return frame->number;
    	else
    		return 0;
    }

    // Set the currently playing frame number (if any)
    void VideoCacheThread::setCurrentFramePosition(int64_t current_frame_number)
    {
    	current_display_frame = current_frame_number;
    }

	// Seek the reader to a particular frame number
	void VideoCacheThread::Seek(int64_t new_position)
	{
		position = new_position;
	}

	// Play the video
	void VideoCacheThread::Play() {
		// Start playing
		is_playing = true;
	}

	// Stop the audio
	void VideoCacheThread::Stop() {
		// Stop playing
		is_playing = false;
	}

    // Start the thread
    void VideoCacheThread::run()
    {
        // Types for storing time durations in whole and fractional milliseconds
        std::shared_ptr<openshot::Frame> smallest_frame = NULL;
        using ms = std::chrono::milliseconds;
        using double_ms = std::chrono::duration<double, ms::period>;

        // Calculate on-screen time for a single frame in milliseconds
        const auto frame_duration = double_ms(1000.0 / reader->info.fps.ToDouble());

		while (!threadShouldExit() && is_playing) {

			// Cache frames before the other threads need them
			// Cache frames up to the max frames. Reset to current position
			// if cache gets too far away from display frame. Cache frames
			// even when player is paused (i.e. speed 0).
			while (((position - current_display_frame) < max_concurrent_frames) && is_playing)
			{
				// Only cache up till the max_concurrent_frames amount... then sleep
				try
				{
					if (reader) {
						ZmqLogger::Instance()->AppendDebugMethod("VideoCacheThread::run (cache frame)", "position", position, "current_display_frame", current_display_frame, "max_concurrent_frames", max_concurrent_frames, "needed_frames", (position - current_display_frame));

						// Force the frame to be generated
                        smallest_frame = reader->GetCache()->GetSmallestFrame();
                        if (smallest_frame && smallest_frame->number > current_display_frame) {
                            // Cache position has gotten too far away from current display frame.
                            // Reset the position to the current display frame.
                            position = current_display_frame;
						}
						reader->GetFrame(position);
					}

				}
				catch (const OutOfBoundsFrame & e)
				{
					// Ignore out of bounds frame exceptions
				}

				// Increment frame number
				position++;
			}

			// Sleep for 1 frame length
			std::this_thread::sleep_for(frame_duration);
		}

	return;
    }
}
