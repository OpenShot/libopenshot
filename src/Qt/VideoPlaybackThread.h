/**
 * @file
 * @brief Source file for VideoPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_VIDEO_PLAYBACK_THREAD_H
#define OPENSHOT_VIDEO_PLAYBACK_THREAD_H

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace openshot
{
    class Frame;
    class RendererBase;
    using juce::Thread;
    using juce::WaitableEvent;

    /**
     *  @brief The video playback class.
     */
    class VideoPlaybackThread : Thread
    {
	RendererBase *renderer;
	std::shared_ptr<Frame> frame;
	WaitableEvent render;
	WaitableEvent rendered;
	bool reset;

	/// Constructor
	VideoPlaybackThread(RendererBase *rb);
	/// Destructor
	~VideoPlaybackThread();

	/// Get the currently playing frame number (if any)
	int64_t getCurrentFramePosition();

	/// Start the thread
	void run();

	/// Parent class of VideoPlaybackThread
	friend class PlayerPrivate;
	friend class QtPlayer;
    };

}

#endif // OPENSHOT_VIDEO_PLAYBACK_THREAD_H
