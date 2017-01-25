/**
 * @file
 * @brief Source file for VideoCacheThread class
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

#include "../../include/Qt/VideoCacheThread.h"

namespace openshot
{
	// Constructor
	VideoCacheThread::VideoCacheThread()
	: Thread("video-cache"), speed(1), is_playing(false), position(1)
	, reader(NULL), max_frames(OPEN_MP_NUM_PROCESSORS * 2), current_display_frame(1)
    {
    }

    // Destructor
	VideoCacheThread::~VideoCacheThread()
    {
    }

    // Get the currently playing frame number (if any)
    long int VideoCacheThread::getCurrentFramePosition()
    {
    	if (frame)
    		return frame->number;
    	else
    		return 0;
    }

    // Set the currently playing frame number (if any)
    void VideoCacheThread::setCurrentFramePosition(long int current_frame_number)
    {
    	current_display_frame = current_frame_number;
    }

	// Seek the reader to a particular frame number
	void VideoCacheThread::Seek(long int new_position)
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
	while (!threadShouldExit() && is_playing) {

		// Calculate sleep time for frame rate
		double frame_time = (1000.0 / reader->info.fps.ToDouble());

	    // Cache frames before the other threads need them
	    // Cache frames up to the max frames
	    while (speed == 1 && (position - current_display_frame) < max_frames)
	    {
	    	// Only cache up till the max_frames amount... then sleep
			try
			{
				if (reader) {
					ZmqLogger::Instance()->AppendDebugMethod("VideoCacheThread::run (cache frame)", "position", position, "current_display_frame", current_display_frame, "max_frames", max_frames, "needed_frames", (position - current_display_frame), "", -1, "", -1);

					// Force the frame to be generated
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
		sleep(frame_time);
	}

	return;
    }
}
