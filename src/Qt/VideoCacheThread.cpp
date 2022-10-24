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
	min_frames_ahead(4), max_frames_ahead(8), should_pause_cache(false),
	timeline_max_frame(0), should_break(false)
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
        // Get timeline instance
        Timeline *t = (Timeline *) reader;

        // Calculate last frame # on timeline (to prevent caching past this point)
        timeline_max_frame = t->GetMaxFrame();

        // Determine previous frame number (depending on last non-zero/non-paused speed)
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
        if (new_position >= 1 && new_position <= timeline_max_frame && !reader->GetCache()->Contains(previous_frame)) {
            // Clear cache
            t->ClearAllCache();

            // Break out of any existing cache loop
            should_break = true;

            // Force cache direction back to forward
            last_speed = 1;
        }

        // Reset pre-roll when requested frame is not currently cached
        if (start_preroll && reader && reader->GetCache() && !reader->GetCache()->Contains(new_position)) {
            // Break out of any existing cache loop
            should_break = true;

            // Reset stats and allow cache to rebuild (if paused)
            cached_frame_count = 0;
            if (speed == 0) {
                should_pause_cache = false;
            }
        }

        // Actually update seek position
        Seek(new_position);
    }

    // Set Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
    void VideoCacheThread::setSpeed(int new_speed) {
        if (new_speed != 0) {
            // Track last non-zero speed
            last_speed = new_speed;
        }
        speed = new_speed;
    }

    // Get the size in bytes of a frame (rough estimate)
    int64_t VideoCacheThread::getBytes(int width, int height, int sample_rate, int channels, float fps)
    {
        int64_t total_bytes = 0;
        total_bytes += static_cast<int64_t>(width * height * sizeof(char) * 4);

        // approximate audio size (sample rate / 24 fps)
        total_bytes += ((sample_rate * channels) / fps) * sizeof(float);

        // return size of this frame
        return total_bytes;
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

        while (!threadShouldExit() && is_playing) {
            // Get settings
            Settings *s = Settings::Instance();

            // init local vars
            min_frames_ahead = s->VIDEO_CACHE_MIN_PREROLL_FRAMES;
            max_frames_ahead = s->VIDEO_CACHE_MAX_PREROLL_FRAMES;

            // Calculate on-screen time for a single frame
            const auto frame_duration = double_micro_sec(1000000.0 / reader->info.fps.ToDouble());
            int current_speed = speed;
            
            // Increment and direction for cache loop
            int64_t increment = 1;

            // Check for empty cache (and re-trigger preroll)
            // This can happen when the user manually empties the timeline cache
            if (reader->GetCache()->Count() == 0) {
                should_pause_cache = false;
                cached_frame_count = 0;
            }

            // Update current display frame
            current_display_frame = requested_display_frame;

            if (current_speed == 0 && should_pause_cache || !s->ENABLE_PLAYBACK_CACHING) {
                // Sleep during pause (after caching additional frames when paused)
                // OR sleep when playback caching is disabled
                std::this_thread::sleep_for(frame_duration / 2);
                continue;

            } else if (current_speed == 0) {
                // Allow 'max frames' to increase when pause is detected (based on cache)
                // To allow the cache to fill-up only on the initial pause.
                should_pause_cache = true;

                // Calculate bytes per frame
                int64_t bytes_per_frame = getBytes(reader->info.width, reader->info.height,
                                                   reader->info.sample_rate, reader->info.channels,
                                                   reader->info.fps.ToFloat());
                Timeline *t = (Timeline *) reader;
                if (t->preview_width != reader->info.width || t->preview_height != reader->info.height) {
                    // If we have a different timeline preview size, use that instead (the preview
                    // window can be smaller, can thus reduce the bytes per frame)
                    bytes_per_frame = getBytes(t->preview_width, t->preview_height,
                                                   reader->info.sample_rate, reader->info.channels,
                                                   reader->info.fps.ToFloat());
                }

                // Calculate # of frames on Timeline cache (when paused)
                if (reader->GetCache() && reader->GetCache()->GetMaxBytes() > 0) {
                    // When paused, limit the cached frames to the following % of total cache size.
                    // This allows for us to leave some cache behind the plahead, and some in front of the playhead.
                    max_frames_ahead = (reader->GetCache()->GetMaxBytes() / bytes_per_frame) * s->VIDEO_CACHE_PERCENT_AHEAD;
                    if (max_frames_ahead > s->VIDEO_CACHE_MAX_FRAMES) {
                        // Ignore values that are too large, and default to a safer value
                        max_frames_ahead = s->VIDEO_CACHE_MAX_FRAMES;
                    }
                }

                // Overwrite the increment to our cache position
                // to fully cache frames while paused (support forward and rewind caching)
                // Use `last_speed` which is the last non-zero/non-paused speed
                if (last_speed < 0) {
                    increment = -1;
                }

            } else {
                // normal playback
                should_pause_cache = false;
            }

            // Always cache frames from the current display position to our maximum (based on the cache size).
            // Frames which are already cached are basically free. Only uncached frames have a big CPU cost.
            // By always looping through the expected frame range, we can fill-in missing frames caused by a
            // fragmented cache object (i.e. the user clicking all over the timeline). The -1 is to always
            // cache 1 frame previous to our current frame (to avoid our Seek method from clearing the cache).
            int64_t starting_frame = std::min(current_display_frame, timeline_max_frame) - 1;
            int64_t ending_frame = std::min(starting_frame + max_frames_ahead, timeline_max_frame);

            // Adjust ending frame for cache loop
            if (increment < 0) {
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

            // Reset cache break-loop flag
            should_break = false;

            // Loop through range of frames (and cache them)
            for (int64_t cache_frame = starting_frame; cache_frame != (ending_frame + increment); cache_frame += increment) {
                cached_frame_count++;
                if (reader && reader->GetCache() && !reader->GetCache()->Contains(cache_frame)) {
                    try
                    {
                        // This frame is not already cached... so request it again (to force the creation & caching)
                        // This will also re-order the missing frame to the front of the cache
                        last_cached_frame = reader->GetFrame(cache_frame);
                    }
                    catch (const OutOfBoundsFrame & e) {  }
                }

                // Check if thread has stopped OR should_break is triggered
                if (!is_playing || should_break || !s->ENABLE_PLAYBACK_CACHING) {
                    should_break = false;
                    break;
                }

            }

			// Sleep for a fraction of frame duration
			std::this_thread::sleep_for(frame_duration / 2);
		}

	return;
    }
}
