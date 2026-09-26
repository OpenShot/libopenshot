/**
 * @file
 * @brief Source file for PlayerPrivate class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "PlayerPrivate.h"
#include "Exceptions.h"

#include <queue>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono microseconds, high_resolution_clock

namespace openshot
{
    int close_to_sync = 5;
    // Constructor
    PlayerPrivate::PlayerPrivate(openshot::RendererBase *rb)
    : Thread("player"), playback_frames(0), video_position(1), audio_position(0),
      reader(nullptr), speed(1), last_speed(1), renderer(rb),
      last_video_position(1), max_sleep_ms(125000), is_dirty(true)
    {
        videoCache = new openshot::VideoCacheThread();
        audioPlayback = new openshot::AudioPlaybackThread(videoCache);
        videoPlayback = new openshot::VideoPlaybackThread(rb);
    }

    // Destructor
    PlayerPrivate::~PlayerPrivate()
    {
        stopPlayback();
        delete audioPlayback;
        delete videoCache;
        delete videoPlayback;
    }

    // Start thread
    void PlayerPrivate::run()
    {
        // bail if no reader set
        if (!reader)
            return;

        // Start the threads
        if (reader->info.has_audio)
            audioPlayback->startThread(Priority::high);
        if (reader->info.has_video) {
            videoCache->startThread(Priority::high);
            videoPlayback->startThread(Priority::high);
        }

        using std::chrono::duration_cast;

        // Types for storing time durations in whole and fractional microseconds
        using micro_sec = std::chrono::microseconds;
        using double_micro_sec = std::chrono::duration<double, micro_sec::period>;

        // Init start_time of playback
        std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> start_time;
        start_time = std::chrono::time_point_cast<micro_sec>(std::chrono::system_clock::now()); ///< timestamp playback starts

        while (!threadShouldExit()) {
            // Calculate on-screen time for a single frame
            int frame_speed = std::max(abs(speed), 1);
            const auto frame_duration = double_micro_sec(1000000.0 / (reader->info.fps.ToDouble() * frame_speed));
            const auto max_sleep = frame_duration * 4; ///< Don't sleep longer than X times a frame duration

            // Pausing Code (which re-syncs audio/video times)
            // - If speed is zero or speed changes
            // - If pre-roll is not ready (This should allow scrubbing of the timeline without waiting on pre-roll)
            bool wait_paused_hold = (speed == 0 && video_position == last_video_position);
            bool wait_speed_change = (speed != 0 && last_speed != speed);
            bool cache_ready = videoCache->isReady();
            bool wait_preroll = (speed != 0 && !is_dirty && !cache_ready);
            bool should_wait = (wait_paused_hold || wait_speed_change || wait_preroll);

            if (should_wait)
            {
                // Sleep for a fraction of frame duration
                std::this_thread::sleep_for(frame_duration / 4);

                // Reset current playback start time
                start_time = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
                playback_frames = 0;
                last_speed = speed;

                // Seek audio thread (since audio is also paused)
                audioPlayback->Seek(video_position);

                continue;
            }

            // Get the current video frame
            const uint64_t frame_seek_generation = seek_generation.load();
            auto frame_to_render = getFrame();
            // A seek during decoding invalidates the old frame. Do not hand
            // that frame to the renderer or overwrite the new playhead.
            if (seek_generation.load() != frame_seek_generation)
                continue;
            const int64_t rendered_position = video_position.load();

            // Set the video frame on the video thread and render frame
            videoPlayback->SetFrame(std::move(frame_to_render));
            videoPlayback->rendered.reset();
            videoPlayback->render.signal();
            // Keep decode/position advancement aligned with actual preview updates.
            // This avoids occasional "silent advance then jump" behavior when
            // preroll transitions and rendering are briefly out-of-sync.
            const int render_wait_ms = std::max(
                1,
                static_cast<int>(frame_duration.count() / 1000.0 * 2.0)
            );
            videoPlayback->rendered.wait(render_wait_ms);

            // Keep track of the last displayed frame
            {
                std::lock_guard<std::mutex> lock(frame_mutex);
                if (seek_generation.load() == frame_seek_generation)
                    last_video_position = rendered_position;
            }
            last_speed = speed;

            // Calculate the diff between 'now' and the predicted frame end time
            const auto current_time = std::chrono::system_clock::now();
            const auto remaining_time = double_micro_sec(start_time +
                    (frame_duration * playback_frames) - current_time);

            // Sleep to display video image on screen
            if (remaining_time > remaining_time.zero() ) {
                if (remaining_time < max_sleep) {
                    std::this_thread::sleep_for(remaining_time);
                } else {
                    // Protect against invalid or too-long sleep times
                    std::this_thread::sleep_for(max_sleep);
                }
            } else {
                // If we're behind schedule (e.g. preroll/render stall), do not
                // burst through delayed frames. Resync timing baseline so
                // playback continues smoothly at normal cadence.
                start_time = std::chrono::time_point_cast<micro_sec>(current_time);
                playback_frames = 0;
            }
        }
    }

    // Get the next displayed frame (based on speed and direction)
    std::shared_ptr<openshot::Frame> PlayerPrivate::getFrame()
    {
	int64_t position;
	uint64_t generation;
	ReaderBase* current_reader;
	{
		std::lock_guard<std::mutex> lock(frame_mutex);
		// Getting new frame, so clear this flag
		is_dirty = false;

		// Get the next frame (based on speed)
		const int current_speed = speed.load();
		const int64_t next_position = video_position.load() + current_speed;
		if (next_position >= 1 && next_position <= reader->info.video_length) {
			video_position = next_position;
		} else if (next_position < 1) {
			video_position = 1;
			speed = 0;
		} else {
			video_position = reader->info.video_length;
			speed = 0;
		}

		position = video_position.load();
		if (frame && frame->number == position && position == last_video_position)
			return frame;
		// Increment playback frames (always in the positive direction)
		playback_frames += std::abs(speed.load());
		// Update cache position before releasing the seek lock.
		videoCache->NotifyPlaybackPosition(position);
		current_reader = reader;
		generation = seek_generation.load();
	}

	// Decoding can take much longer than a frame. Keep Seek responsive while
	// it runs, then publish only if no newer seek invalidated the result.
	std::shared_ptr<Frame> decoded;
	try {
		decoded = current_reader->GetFrame(position);
	} catch (const ReaderClosed &) {
	} catch (const OutOfBoundsFrame &) {
	}
	{
		std::lock_guard<std::mutex> lock(frame_mutex);
		if (seek_generation.load() != generation)
			return {};
		frame = std::move(decoded);
		return frame;
	}
    }

    // Seek to a new position
    void PlayerPrivate::Seek(int64_t new_position)
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        ++seek_generation;
        video_position = new_position;
        last_video_position = 0;
        // Drop local frame reference so same-frame refreshes cannot reuse stale
        // content after timeline/clip property updates.
        frame.reset();
        // Always force immediate refresh after seek/update, even while playing.
        is_dirty = true;
    }

    // Start video/audio playback
    bool PlayerPrivate::startPlayback()
    {
        if (video_position < 0) return false;

        stopPlayback();
        startThread(Priority::high);
        return true;
    }

    // Stop video/audio playback
    void PlayerPrivate::stopPlayback()
    {
        if (videoCache->isThreadRunning() && reader->info.has_video) videoCache->stopThread(max_sleep_ms);
        if (audioPlayback->isThreadRunning() && reader->info.has_audio) audioPlayback->stopThread(max_sleep_ms);
        if (videoPlayback->isThreadRunning() && reader->info.has_video) videoPlayback->stopThread(max_sleep_ms);
        if (isThreadRunning()) stopThread(max_sleep_ms);
    }

}
