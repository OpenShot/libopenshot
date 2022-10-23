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
    : renderer(rb), Thread("player"), video_position(1), audio_position(0),
      speed(1), reader(NULL), last_video_position(1), max_sleep_ms(125000), playback_frames(0), is_dirty(true)
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
            audioPlayback->startThread(8);
        if (reader->info.has_video) {
            videoCache->startThread(2);
            videoPlayback->startThread(4);
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
            if ((speed == 0 && video_position == last_video_position) ||
                (speed != 0 && last_speed != speed) ||
                (speed != 0 && !is_dirty && !videoCache->isReady()))
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
            frame = getFrame();

            // Set the video frame on the video thread and render frame
            videoPlayback->frame = frame;
            videoPlayback->render.signal();

            // Keep track of the last displayed frame
            last_video_position = video_position;
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
            }
        }
    }

    // Get the next displayed frame (based on speed and direction)
    std::shared_ptr<openshot::Frame> PlayerPrivate::getFrame()
    {
    try {
        // Getting new frame, so clear this flag
        is_dirty = false;

        // Get the next frame (based on speed)
        if (video_position + speed >= 1 && video_position + speed <= reader->info.video_length) {
            video_position = video_position + speed;

        } else if (video_position + speed < 1) {
            // Start of reader (prevent negative frame number and pause playback)
            video_position = 1;
            speed = 0;
        } else if (video_position + speed > reader->info.video_length) {
            // End of reader (prevent negative frame number and pause playback)
            video_position = reader->info.video_length;
            speed = 0;
        }

        if (frame && frame->number == video_position && video_position == last_video_position) {
            // return cached frame
            return frame;
        }
        else
        {
            // Increment playback frames (always in the positive direction)
            playback_frames += std::abs(speed);

            // Update cache on which frame was retrieved
            videoCache->Seek(video_position);

            // return frame from reader
            return reader->GetFrame(video_position);
        }

    } catch (const ReaderClosed & e) {
        // ...
    } catch (const OutOfBoundsFrame & e) {
        // ...
    }
    return std::shared_ptr<openshot::Frame>();
    }

    // Seek to a new position
    void PlayerPrivate::Seek(int64_t new_position)
    {
        video_position = new_position;
        last_video_position = 0;
        is_dirty = true;
    }

    // Start video/audio playback
    bool PlayerPrivate::startPlayback()
    {
        if (video_position < 0) return false;

        stopPlayback();
        startThread(1);
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
