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
#include "Settings.h"
#include "Timeline.h"

#include <algorithm>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono::microseconds

namespace openshot
{
	// Constructor
	VideoCacheThread::VideoCacheThread()
	: Thread("video-cache"), speed(0), last_speed(1), is_playing(false),
	reader(NULL), current_display_frame(1), cached_frame_count(0),
	min_frames_ahead(4), max_frames_ahead(8), should_pause_cache(false)
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
        // Determine previous frame number (depending on speed)
        int64_t previous_frame = new_position;
        if (last_speed < 0) {
            // backwards
            previous_frame++;
        } else if (last_speed > 0) {
            // forward
            previous_frame--;
        }
        if (previous_frame <= 0) {
            // min frame is 1
            previous_frame = 1;
        }

        // Clear cache if previous frame outside the cached range, which means we are
        // requesting a non-contigous frame compared to our current cache range
        if (!reader->GetCache()->Contains(previous_frame)) {
            Timeline *t = (Timeline *) reader;
            t->ClearAllCache();
        }

        // Reset pre-roll when requested frame is not currently cached
        if (start_preroll && reader && reader->GetCache() && !reader->GetCache()->Contains(new_position)) {
            cached_frame_count = 0;
            if (speed == 0) {
                should_pause_cache = false;
            }
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
        // Get settings
        Settings *s = Settings::Instance();

        // Types for storing time durations in whole and fractional microseconds
        using micro_sec = std::chrono::microseconds;
        using double_micro_sec = std::chrono::duration<double, micro_sec::period>;

        while (!threadShouldExit() && is_playing) {
            // init local vars
            min_frames_ahead = s->VIDEO_CACHE_MIN_PREROLL_FRAMES;
            max_frames_ahead = s->VIDEO_CACHE_MAX_PREROLL_FRAMES;

            // Calculate on-screen time for a single frame
            const auto frame_duration = double_micro_sec(1000000.0 / reader->info.fps.ToDouble());
            int current_speed = speed;
            
            // Check for empty cache (and re-trigger preroll)
            // This can happen when the user manually empties the timeline cache
            if (reader->GetCache()->Count() == 0) {
                should_pause_cache = false;
                cached_frame_count = 0;
            }

            // Calculate increment (based on current_speed)
            // Support caching in both directions
            int16_t increment = current_speed;

            if (current_speed == 0 && should_pause_cache || !s->ENABLE_PLAYBACK_CACHING) {
                // Sleep during pause (after caching additional frames when paused)
                // OR sleep when playback caching is disabled
                current_display_frame = requested_display_frame;
                std::this_thread::sleep_for(frame_duration / 2);
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
                    if (max_frames_ahead > s->VIDEO_CACHE_MAX_FRAMES) {
                        // Ignore values that are too large, and default to a safer value
                        max_frames_ahead = s->VIDEO_CACHE_MAX_FRAMES;
                    }
                }

                // Overwrite the increment to our cache position
                // to fully cache frames while paused (support forward and rewind caching)
                if (last_speed > 0) {
                    increment = 1;
                } else {
                    increment = -1;
                }

            } else {
                // normal playback
                should_pause_cache = false;
            }

			// Always cache frames from the current display position to our maximum (based on the cache size).
			// Frames which are already cached are basically free. Only uncached frames have a big CPU cost.
			// By always looping through the expected frame range, we can fill-in missing frames caused by a
			// fragmented cache object (i.e. the user clicking all over the timeline).
            int64_t starting_frame = current_display_frame;
            int64_t ending_frame = starting_frame + max_frames_ahead;

            // Adjust ending frame for cache loop
            if (last_speed < 0) {
                // Reverse loop (if we are going backwards)
                ending_frame = starting_frame - max_frames_ahead;
            }
            if (starting_frame < 1) {
                // Don't allow negative frame number caching
                starting_frame = 1;
            }
            if (ending_frame < 1) {
                // Don't allow negative frame number caching
                ending_frame = 1;
            }

            // Loop through range of frames (and cache them)
            int64_t uncached_frame_count = 0;
            int64_t already_cached_frame_count = 0;
            for (int64_t cache_frame = starting_frame; cache_frame != (ending_frame + increment); cache_frame += increment) {
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
                        should_pause_cache = false;
                        break;
                    } else if (speed < 0 && (requested_display_frame > starting_frame || requested_display_frame < ending_frame)) {
                        should_pause_cache = false;
                        break;
                    }
                }
                // Check if playback speed changed (if so, break out of cache loop)
                if (current_speed != speed) {
                    break;
                }
                // Check if playback has stopped
                if (!is_playing) {
                    break;
                }
            }

            // Update cache counts
            if (current_speed == 1 && cached_frame_count > max_frames_ahead && uncached_frame_count > min_frames_ahead) {
                // start cached count again (we have too many uncached frames)
                cached_frame_count = 0;
            }

            // Update current display frame & last non-paused speed
            current_display_frame = requested_display_frame;
            if (current_speed != 0) {
                last_speed = current_speed;
            }

			// Sleep for a fraction of frame duration
			std::this_thread::sleep_for(frame_duration / 2);
		}

	return;
    }
}
