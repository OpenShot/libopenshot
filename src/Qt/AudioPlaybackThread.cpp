/**
 * @file
 * @brief Source file for AudioPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
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
#include "AudioPlaybackThread.h"

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

    AudioPlaybackThread::AudioPlaybackThread()
	: Thread("audio-playback")
	, audioDeviceManager()
	, player()
	, transport()
	, mixer()
	, source(NULL)
	, sampleRate(0.0)
	, numChannels(0)
    {
    }

    AudioPlaybackThread::~AudioPlaybackThread()
    {
    }

    void AudioPlaybackThread::setReader(ReaderBase *reader)
    {
	sampleRate = reader->info.sample_rate;
	numChannels = reader->info.channels;
	source = new AudioReaderSource(reader, 1, 10000);
    }

    tr1::shared_ptr<Frame> AudioPlaybackThread::getFrame()
    {
	if (source) return source->getFrame();
	return tr1::shared_ptr<Frame>();
    }

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
	    10000, // tells it to buffer this many samples ahead
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

	//TODO: re-add this code    !threadShouldExit() &&
	while (transport.isPlaying()) {
		cout << "... still playing" << endl;
	    sleep(1);
	}

	transport.stop();
	transport.setSource(0);

	player.setSource(0);
	audioDeviceManager.removeAudioCallback(&player);
	audioDeviceManager.closeAudioDevice();
	audioDeviceManager.removeAllChangeListeners();
	audioDeviceManager.dispatchPendingMessages();
    }
}
