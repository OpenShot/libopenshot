/**
 * @file
 * @brief Source file for VideoPlaybackThread class
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

namespace openshot
{
    using juce::Thread;
    using juce::WaitableEvent;

    /**
     *  @brief The video playback class.
     */
    class VideoPlaybackThread : Thread
    {
	RendererBase *renderer;
	std::tr1::shared_ptr<Frame> frame;
	WaitableEvent render;
	WaitableEvent rendered;
	bool reset;

	/// Constructor
	VideoPlaybackThread(RendererBase *rb);
	/// Destructor
	~VideoPlaybackThread();

	/// Get the currently playing frame number (if any)
	int getCurrentFramePosition();

	/// Start the thread
	void run();

	/// Parent class of VideoPlaybackThread
	friend class PlayerPrivate;
    };

}
