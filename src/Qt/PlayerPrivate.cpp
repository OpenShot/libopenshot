/**
 * @file
 * @brief Source file for PlayerPrivate class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "../include/ReaderBase.h"
#include "../include/RendererBase.h"
#include "../include/AudioReaderSource.h"
#include "PlayerPrivate.h"
#include "AudioPlaybackThread.h"
#include "VideoPlaybackThread.h"

namespace openshot
{
    PlayerPrivate::PlayerPrivate(RendererBase *rb)
	: Thread("player"), video_position(0), audio_position(0)
	, audioPlayback(new AudioPlaybackThread())
	, videoPlayback(new VideoPlaybackThread(rb))
    {
    }

    PlayerPrivate::~PlayerPrivate()
    {
	if (isThreadRunning()) stopThread(500);
	if (audioPlayback->isThreadRunning() && reader->info.has_audio) audioPlayback->stopThread(500);
	if (videoPlayback->isThreadRunning() && reader->info.has_video) videoPlayback->stopThread(500);
	delete audioPlayback;
	delete videoPlayback;
    }

    void PlayerPrivate::run()
    {
	if (audioPlayback->isThreadRunning() && reader->info.has_audio) audioPlayback->stopThread(-1);
	if (videoPlayback->isThreadRunning() && reader->info.has_video) videoPlayback->stopThread(-1);

	// Set the reader for the Audio thread
	audioPlayback->setReader(reader);

	// Start the threads
	if (reader->info.has_audio)
		audioPlayback->startThread(1);
	if (reader->info.has_video)
		videoPlayback->startThread(2);

	tr1::shared_ptr<Frame> frame;
	while (!threadShouldExit()) {

		// Get the start time (to track how long a frame takes to render)
	    const Time t1 = Time::getCurrentTime();

	    // Get the current video frame
	    frame = getFrame();

	    // Set the video frame on the video thread and render frame
	    videoPlayback->frame = frame;
	    videoPlayback->render.signal();
	    videoPlayback->rendered.wait();

	    // How many frames ahead or behind is the video thread?
	    int video_frame_diff = 0;
	    if (reader->info.has_audio && reader->info.has_video)
	    	// Only calculate this if a reader contains both an audio and video thread
	    	audio_position = audioPlayback->getCurrentFramePosition();
	    	video_frame_diff = video_position - audio_position;

	    // Get the end time (to track how long a frame takes to render)
	    const Time t2 = Time::getCurrentTime();

	    // Calculate the milliseconds a single frame should stay on the screen
	    double frame_time = (1000.0 / reader->info.fps.ToDouble());

	    // Determine how many milliseconds it took to render the frame
	    int64 render_time = t2.toMilliseconds() - t1.toMilliseconds();

	    // Calculate the amount of time to sleep (by subtracting the render time)
	    int sleep_time = int(frame_time - render_time);

	    // Adjust drift (if more than a few frames off between audio and video)
	    if (video_frame_diff > 0 && reader->info.has_audio && reader->info.has_video)
	    	// Since the audio and video threads are running independently, they will quickly get out of sync.
	    	// To fix this, we calculate how far ahead or behind the video frame is, and adjust the amount of time
	    	// the frame is displayed on the screen (i.e. the sleep time). If a frame is ahead of the audio,
	    	// we sleep for longer. If a frame is behind the audio, we sleep less (or not at all), in order for
	    	// the video to catch up.
	    	sleep_time += (video_frame_diff * (1000.0 / reader->info.fps.ToDouble()));

	    // Sleep (leaving the video frame on the screen for the correct amount of time)
	    if (sleep_time > 0) sleep(sleep_time);

	    // Debug output
	    std::cout << "video frame diff: " << video_frame_diff << std::endl;

	    // Determine if the next frame will be the end of stream
	    //if ((video_position + 1) > reader->info.video_length)
	    //{
	    	// End threads at END OF STREAM
	    	//if (reader->info.has_audio)
	    	//	audioPlayback->stopThread(1);
	    	//if (reader->info.has_video)
	    	//	videoPlayback->stopThread(2);
	    //}
	}
	
	if (audioPlayback->isThreadRunning() && reader->info.has_audio) audioPlayback->stopThread(-1);
	if (videoPlayback->isThreadRunning() && reader->info.has_video) videoPlayback->stopThread(-1);
    }

    tr1::shared_ptr<Frame> PlayerPrivate::getFrame()
    {
	try {
	    return reader->GetFrameSafe(video_position++);
	} catch (const ReaderClosed & e) {
	    // ...
	} catch (const TooManySeeks & e) {
	    // ...
	} catch (const OutOfBoundsFrame & e) {
	    // ...
	}
	return tr1::shared_ptr<Frame>();
    }

    bool PlayerPrivate::startPlayback()
    {
	if (video_position < 0) return false;
	stopPlayback(-1);
	startThread(1);
	return true;
    }

    void PlayerPrivate::stopPlayback(int timeOutMilliseconds)
    {
	if (isThreadRunning()) stopThread(timeOutMilliseconds);
    }

}
