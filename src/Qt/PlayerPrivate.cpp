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

#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono microseconds, high_resolution_clock
#include <cmath>

namespace openshot
{
    const auto max_sleep = std::chrono::seconds(1);
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

        while (!threadShouldExit()) {
            const auto time1 = std::chrono::high_resolution_clock::now();
            const auto frame_duration = double_micro_sec(1000000.0 / reader->info.fps.ToDouble());
            frame = getFrame();
            if ((speed == 0 && video_position == last_video_position)
                || (video_position > reader->info.video_length)
               ) {
                speed = 0;
                std::this_thread::sleep_for(frame_duration);
                continue;
            }
            //Display the frame
            videoPlayback->frame = frame;
            videoPlayback->render.signal();
            last_video_position = video_position;

            // How many frames ahead or behind is the video thread?
            int64_t video_frame_diff = 0;
            if (reader->info.has_audio && reader->info.has_video) {
                if (speed != 1)
                    // Set audio frame again (since we are not in normal speed, and not paused)
                    audioPlayback->Seek(video_position);

                // Only calculate this if a reader contains both an audio and video thread
                audio_position = audioPlayback->getCurrentFramePosition();
                video_frame_diff = video_position - audio_position;
            }

            // how far ahead or behind (negative) the video is
            // relative to the audio
            // (Cubed so that the higher the error, the more extreme the correction
            auto correction = micro_sec((long)frame_duration.count() * (long)powf(video_frame_diff,3));

            // If we're more than a whole frame behind, skip frames
            int frames_behind = correction.count() / frame_duration.count();
            if (frames_behind > 0) {
                videoPlayback->frame->number += frames_behind;
            }

            /* SLEEP CALCULATIONS */
            const auto time2 = std::chrono::high_resolution_clock::now();
            const auto render_time = double_micro_sec(time2-time1);
            const auto sleep_time = duration_cast<micro_sec>(frame_duration - render_time) + (correction);

            if (sleep_time.count() > 0) {
                if (sleep_time > max_sleep ) {
                    std::this_thread::sleep_for(max_sleep);
                } else {
                    std::this_thread::sleep_for(sleep_time);
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
            // Update cache on which frame was retrieved
            videoCache->setCurrentFramePosition(video_position);

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
