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

#include <algorithm>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono::microseconds

namespace openshot
{
	// Constructor
	VideoCacheThread::VideoCacheThread()
	: Thread("video-cache"), speed(0), last_speed(1), is_playing(false),
	reader(NULL), current_display_frame(1), cached_frame_count(0),
	min_frames_ahead(4), max_frames_ahead(8)
    {
    }

    // Destructor
	VideoCacheThread::~VideoCacheThread()
    {
    }

	// Seek the reader to a particular frame number
	void VideoCacheThread::Seek(int64_t new_position)
	{
        requested_display_frame = new_position;
	}

    // Seek the reader to a particular frame number and optionally start the pre-roll
    void VideoCacheThread::Seek(int64_t new_position, bool start_preroll)
    {
	    if (start_preroll && reader && reader->GetCache() && !reader->GetCache()->Contains(new_position)) {
            cached_frame_count = 0;
	    }
        Seek(new_position);
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

	// Is cache ready for playback (pre-roll)
    bool VideoCacheThread::isReady() {
	    return (cached_frame_count > min_frames_ahead);
	}

    // Start the thread
    void VideoCacheThread::run()
    {
        // Types for storing time durations in whole and fractional microseconds
        using micro_sec = std::chrono::microseconds;
        using double_micro_sec = std::chrono::duration<double, micro_sec::period>;
        bool should_pause_cache = false;

		while (!threadShouldExit() && is_playing) {
            // Calculate on-screen time for a single frame
            const auto frame_duration = double_micro_sec(1000000.0 / reader->info.fps.ToDouble());
            int current_speed = speed;

            // Calculate increment (based on speed)
            // Support caching in both directions
            int16_t increment = speed;
            if (current_speed == 0 && should_pause_cache) {
                // Sleep during pause (after caching additional frames when paused)
                std::this_thread::sleep_for(frame_duration / 4);
                continue;

            } else if (current_speed == 0) {
                // Allow 'max frames' to increase when pause is detected (based on cache)
                // To allow the cache to fill-up only on the initial pause.
                should_pause_cache = true;

                // Calculate bytes per frame. If we have a reference openshot::Frame, use that instead (the preview
                // window can be smaller, can thus reduce the bytes per frame)
                int64_t bytes_per_frame = (reader->info.height * reader->info.width * 4) +
                                          (reader->info.sample_rate * reader->info.channels * 4);
                if (last_cached_frame && last_cached_frame->has_image_data && last_cached_frame->has_audio_data) {
                    bytes_per_frame = last_cached_frame->GetBytes();
                }

                // Calculate # of frames on Timeline cache (when paused)
                if (reader->GetCache() && reader->GetCache()->GetMaxBytes() > 0) {
                    // When paused, use 1/2 the cache size (so our cache will be 50% before the play-head, and 50% after it)
                    max_frames_ahead = (reader->GetCache()->GetMaxBytes() / bytes_per_frame) / 2;
                    if (max_frames_ahead > 300) {
                        // Ignore values that are too large, and default to a safer value
                        max_frames_ahead = 300;
                    }
                }

                // Overwrite the increment to our cache position
                // to fully cache frames while paused (support forward and rewind)
                if (last_speed > 0) {
                    increment = 1;
                } else {
                    increment = -1;
                }

            } else {
                // Default max frames ahead (normal playback)
                max_frames_ahead = 8;
                should_pause_cache = false;
            }

			// Always cache frames from the current display position to our maximum (based on the cache size).
			// Frames which are already cached are basically free. Only uncached frames have a big CPU cost.
			// By always looping through the expected frame range, we can fill-in missing frames caused by a
			// fragmented cache object (i.e. the user clicking all over the timeline).
            int64_t starting_frame = current_display_frame;
            int64_t ending_frame = starting_frame + max_frames_ahead;

            // Adjust ending frame for cache loop
            if (speed < 0) {
                // Reverse loop (if we are going backwards)
                ending_frame = starting_frame - max_frames_ahead;
            }
            if (ending_frame < 0) {
                // Don't allow negative frame number caching
                ending_frame = 0;
            }

            // Loop through range of frames (and cache them)
            int64_t uncached_frame_count = 0;
            int64_t already_cached_frame_count = 0;
            for (int64_t cache_frame = starting_frame; cache_frame != ending_frame; cache_frame += increment) {
                cached_frame_count++;
                if (reader && reader->GetCache() && !reader->GetCache()->Contains(cache_frame)) {
                    try
                    {
                        // This frame is not already cached... so request it again (to force the creation & caching)
                        // This will also re-order the missing frame to the front of the cache
                        last_cached_frame = reader->GetFrame(cache_frame);
                        uncached_frame_count++;
                    }
                    catch (const OutOfBoundsFrame & e) {  }
                } else if (reader && reader->GetCache() && reader->GetCache()->Contains(cache_frame)) {
                    already_cached_frame_count++;
                }

                // Check if the user has seeked outside the cache range
                if (requested_display_frame != current_display_frame) {
                    // cache will restart at a new position
                    if (speed >= 0 && (requested_display_frame < starting_frame || requested_display_frame > ending_frame)) {
                        break;
                    } else if (speed < 0 && (requested_display_frame > starting_frame || requested_display_frame < ending_frame)) {
                        break;
                    }
                }
                // Check if playback speed changed (if so, break out of cache loop)
                if (current_speed != speed) {
                    break;
                }
            }

            // Update cache counts
            if (cached_frame_count > max_frames_ahead && uncached_frame_count > (min_frames_ahead / 4)) {
                // start cached count again (we have too many uncached frames)
                cached_frame_count = 0;
            }

            // Update current display frame & last non-paused speed
            current_display_frame = requested_display_frame;
            if (current_speed != 0) {
                last_speed = current_speed;
            }

			// Sleep for a fraction of frame duration
			std::this_thread::sleep_for(frame_duration / 4);
		}

	return;
    }
}
