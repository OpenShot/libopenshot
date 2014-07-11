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
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
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
    , is_playing(false)
    {
    }

    // Destructor
    AudioPlaybackThread::~AudioPlaybackThread()
    {
    }

    // Set the reader object
    void AudioPlaybackThread::Reader(ReaderBase *reader)
    {
		// Stop any existing audio
		if (source) {
			// Stop playing
			Stop();
			transport.stop();

			// Wait for transport to stop
			while (transport.isPlaying()) {
				cout << "waiting for transport to stop" << endl;
			    sleep(25);
			}

			// Kill previous audio
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

		// Set local vars
		sampleRate = reader->info.sample_rate;
		numChannels = reader->info.channels;

		// Create new audio source reader
		source = new AudioReaderSource(reader, 1, buffer_size);

		// Play the video
		Play();

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

	// Play the audio
	void AudioPlaybackThread::Play() {
		// Start playing
		is_playing = true;
	}

	// Stop the audio
	void AudioPlaybackThread::Stop() {
		// Stop playing
		is_playing = false;
	}

	// Start audio thread
    void AudioPlaybackThread::run()
    {
    	while (!threadShouldExit())
    	{
    		if (source && !transport.isPlaying() && is_playing) {

    			// Start new audio device
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

    			// Start playing
    			is_playing = true;

    			// Start the transport
    			transport.start();

    			while (!threadShouldExit() && transport.isPlaying() && is_playing) {
    				sleep(250);
    			}

    			is_playing = false;
    		}

    		sleep(250);
    	}

    }
}
