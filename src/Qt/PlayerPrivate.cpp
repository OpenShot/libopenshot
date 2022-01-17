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
    int close_to_sync = 5;
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
        auto sleep_adjustment = double_micro_sec(0.0);
        std::deque<int64_t> video_diff_history;

        // Init sleep adjustment code
        float average_video_diff = 0;       ///< Average number of frames difference between audio and video thread
        bool enable_adjustment = false;     ///< Toggle de-sync adjustments ON & OFF (i.e. only adjust when needed)
        float video_diff_threshold = 1.0;   ///< Number of frames difference to trigger an adjustment

        // DEBUG
        std::cout << "average_video_diff\tsleep_adjustment\tsleep_time" << std::endl;

        while (!threadShouldExit()) {
            // Get the start time (to track how long a frame takes to render)
            const auto time1 = std::chrono::high_resolution_clock::now();

            // Calculate on-screen time for a single frame in microseconds
            const auto frame_duration = double_micro_sec(1000000.0 / reader->info.fps.ToDouble());
            const auto max_sleep = frame_duration * 4; ///< Don't sleep longer than X times a frame duration
            const auto sleep_adjustment_increment = frame_duration / 10000; ///< Tiny fraction of frame duration

            // Adjust size used in averaging desync calculation
            auto history_size = reader->info.fps.ToInt(); ///< How many values should we average together (i.e. use FPS)

            // Get the current video frame (if it's different)
            frame = getFrame();

            // Experimental Pausing Code (if frame has not changed)
            if ((speed == 0 && video_position == last_video_position) || (video_position > reader->info.video_length))
            {
                speed = 0;
                // Clear sleep adjustments & average diff
                sleep_adjustment = double_micro_sec(0.0);
                average_video_diff = 0.0;
                video_diff_history.clear();
                // Sleep for 1 frame
                std::this_thread::sleep_for(frame_duration);
                continue;
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

            // Add video diff to history (for averaging sync delays over time)
            video_diff_history.push_front(video_frame_diff);
            if (video_diff_history.size() > history_size) {
                // Remove oldest element (keep queue frozen with only most recent diffs)
                video_diff_history.pop_back();
                // Loop through recent video diff values
                int64_t diff_sum = 0;
                for(auto it=video_diff_history.begin(); it!=video_diff_history.end();++it) {
                    diff_sum += *it;
                }
                // Calculate average video diff (to be used for syncing video to audio)
                average_video_diff = float(diff_sum) / history_size;
            }

            // Should we enable the adjustment? (i.e. too much desync)
            if (!enable_adjustment && fabs(average_video_diff) > video_diff_threshold) {
                enable_adjustment = true;
            } else if (enable_adjustment && fabs(average_video_diff) > 0.0 && fabs(average_video_diff) < 0.25) {
                // Close to zero, stop adjusting
                enable_adjustment = false;
            }

            // Calculate sync adjustment (if adjustment is triggered)
            if (enable_adjustment) {
                if (average_video_diff < 0.0) {
                    // We need to slowly decrease the sleep
                    sleep_adjustment -= sleep_adjustment_increment;
                } else if (video_frame_diff > 0.0) {
                    // We need to slowly increase the sleep
                    sleep_adjustment += sleep_adjustment_increment;
                }
            }

            // Get the end time (to track how long a frame takes to render)
            const auto time2 = std::chrono::high_resolution_clock::now();

            // Determine how many microseconds it took to render the frame
            const auto render_time = double_micro_sec(time2 - time1);

            // Calculate the amount of time to sleep (by subtracting the render time)
            auto sleep_time = duration_cast<micro_sec>(frame_duration - render_time + sleep_adjustment);

            // DEBUG
            std::cout << average_video_diff << "\t" << sleep_adjustment.count() << "\t" << sleep_time.count() << std::endl;

            // Sleep if sleep_time is greater than zero after correction
            // (leaving the video frame on the screen for the correct amount of time)
            if (sleep_time > sleep_time.zero() ) {
                if (sleep_time < max_sleep) {
                    std::this_thread::sleep_for(sleep_time);
                } else {
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
