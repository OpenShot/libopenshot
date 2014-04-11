/**
 * @file
 * @brief Source file for AudioPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org> *
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#include "../../include/Qt/AudioPlaybackThread.h"

namespace openshot
{
    struct SafeTimeSliceThread : TimeSliceThread
    {
	SafeTimeSliceThread(const String & s) : TimeSliceThread(s) {}
	void run()
	{
	    try {
		TimeSliceThread::run();
	    } catch (const TooManySeeks & e) {
		// ...
	    }
	}
    };

    // Construtor
    AudioPlaybackThread::AudioPlaybackThread()
	: Thread("audio-playback")
	, audioDeviceManager()
	, player()
	, transport()
	, mixer()
	, source(NULL)
	, sampleRate(0.0)
	, numChannels(0)
    , buffer_size(10000)
    {
    }

    // Destructor
    AudioPlaybackThread::~AudioPlaybackThread()
    {
    }

    // Set the reader object
    void AudioPlaybackThread::Reader(ReaderBase *reader)
    {
    	if (!source) {
			sampleRate = reader->info.sample_rate;
			numChannels = reader->info.channels;
			source = new AudioReaderSource(reader, 1, buffer_size);
    	}
    }

    // Get the current frame object (which is filling the buffer)
    tr1::shared_ptr<Frame> AudioPlaybackThread::getFrame()
    {
	if (source) return source->getFrame();
	return tr1::shared_ptr<Frame>();
    }

    // Get the currently playing frame number
    int AudioPlaybackThread::getCurrentFramePosition()
    {
	return source ? source->getEstimatedFrame() : 0;
    }

	// Seek the audio thread
	void AudioPlaybackThread::Seek(int new_position)
	{
		source->Seek(new_position);
	}

	// Start audio thread
    void AudioPlaybackThread::run()
    {
	// Init audio device
	audioDeviceManager.initialise (
	    0, /* number of input channels */
	    numChannels, /* number of output channels */
	    0, /* no XML settings.. */
	    true  /* select default device on failure */);

	// Add callback
	audioDeviceManager.addAudioCallback(&player);

	// Create TimeSliceThread for audio buffering
	SafeTimeSliceThread thread("audio-buffer");
	thread.startThread();

	// Connect source to transport
	transport.setSource(
	    source,
	    buffer_size, // tells it to buffer this many samples ahead
	    &thread,
	    sampleRate,
	    numChannels);
	transport.setPosition(0);
	transport.setGain(1.0);

	// Connect transport to mixer and player
	mixer.addInputSource(&transport, false);
	player.setSource(&mixer);

	cout << "starting transport" << endl;
	transport.start();

	while (!threadShouldExit() && transport.isPlaying()) {
	    sleep(100);
	}

	transport.stop();
	transport.setSource(0);

	player.setSource(0);
	audioDeviceManager.removeAudioCallback(&player);
	audioDeviceManager.closeAudioDevice();
	audioDeviceManager.removeAllChangeListeners();
	audioDeviceManager.dispatchPendingMessages();

	// Remove source
	delete source;
	source = NULL;
    }
}
