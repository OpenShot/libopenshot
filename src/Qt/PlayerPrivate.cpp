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
#include "ZmqLogger.h"

#include <queue>
#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono milliseconds, high_resolution_clock

namespace openshot
{
    bool paused = true; // Used to know when the first 'un-paused' loop is.
    // Constructor
    PlayerPrivate::PlayerPrivate(openshot::RendererBase *rb)
    : renderer(rb), Thread("player"), video_position(1), audio_position(0)
    , audioPlayback(new openshot::AudioPlaybackThread())
    , videoPlayback(new openshot::VideoPlaybackThread(rb))
    , videoCache(new openshot::VideoCacheThread())
    , speed(1), reader(NULL), last_video_position(1), max_sleep_ms(125000)
    { }

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

        // Time-based video sync
        auto start_time = std::chrono::high_resolution_clock::now(); ///< timestamp playback starts

        while (!threadShouldExit()) {
            // Calculate on-screen time for a single frame
            const auto frame_duration = double_micro_sec(1000000.0 / reader->info.fps.ToDouble());
            const auto max_sleep = frame_duration * 4; ///< Don't sleep longer than X times a frame duration

            // Get the current video frame (if it's different)
            frame = getFrame();

            // Pausing Code (if frame has not changed)
            if ((speed == 0 && video_position == last_video_position) || (video_position > reader->info.video_length))
            {
                paused = true;
                // Set start time to prepare for next playback period and reset frame counter
                start_time = std::chrono::high_resolution_clock::now();
                playback_frames = 0;

                // Sleep for a fraction of frame duration
                std::this_thread::sleep_for(frame_duration / 4);
                continue;
            } else if (paused) {
                start_time = std::chrono::high_resolution_clock::now();
                auto adjustment = std::chrono::milliseconds(int(audioPlayback->source->getReaderInfo().duration));
                start_time += adjustment;
                paused = false;
            }

            // Set the video frame on the video thread and render frame
            videoPlayback->frame = frame;
            videoPlayback->render.signal();

            // Keep track of the last displayed frame
            last_video_position = video_position;

            // How many frames ahead or behind is the video thread?
            // Only calculate this if a reader contains both an audio and video thread
            int64_t video_frame_diff = 0;
            if (reader->info.has_audio && reader->info.has_video) {
                audio_position = audioPlayback->getCurrentFramePosition();
                video_frame_diff = video_position - audio_position;
                // Seek to audio frame again (since we are not in normal speed, and not paused)
                if (speed != 1) {
                    audioPlayback->Seek(video_position);
                }
            }

            // Calculate the diff between 'now' and the predicted frame end time
            const auto current_time = std::chrono::high_resolution_clock::now();
            const auto remaining_time = double_micro_sec(start_time + (frame_duration * playback_frames) - current_time);

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
        // Get the next frame (based on speed)
        if (video_position + speed >= 1 && video_position + speed <= reader->info.video_length)
            video_position = video_position + speed;

        if (frame && frame->number == video_position && video_position == last_video_position) {
            // return cached frame
            return frame;
        }
        else
        {
            // Increment playback frames
            playback_frames += speed;

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
        if (audioPlayback->isThreadRunning() && reader->info.has_audio) audioPlayback->stopThread(max_sleep_ms);
        if (videoCache->isThreadRunning() && reader->info.has_video) videoCache->stopThread(max_sleep_ms);
        if (videoPlayback->isThreadRunning() && reader->info.has_video) videoPlayback->stopThread(max_sleep_ms);
        if (isThreadRunning()) stopThread(max_sleep_ms);
    }

}
