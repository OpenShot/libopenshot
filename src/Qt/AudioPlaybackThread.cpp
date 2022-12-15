/**
 * @file
 * @brief Source file for AudioPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org> *
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioPlaybackThread.h"
#include "Settings.h"

#include "../ReaderBase.h"
#include "../RendererBase.h"
#include "../AudioReaderSource.h"
#include "../AudioDevices.h"
#include "../Settings.h"

#include <mutex>
#include <thread>	// for std::this_thread::sleep_for
#include <chrono>	// for std::chrono::milliseconds

using namespace juce;

namespace openshot
{
	// Global reference to device manager
	AudioDeviceManagerSingleton *AudioDeviceManagerSingleton::m_pInstance = NULL;

	// Create or Get audio device singleton with default settings (44100, 2)
	AudioDeviceManagerSingleton *AudioDeviceManagerSingleton::Instance()
	{
		return AudioDeviceManagerSingleton::Instance(44100, 2);
	}

	// Create or Get an instance of the device manager singleton (with custom sample rate & channels)
	AudioDeviceManagerSingleton *AudioDeviceManagerSingleton::Instance(int rate, int channels)
	{
		static std::mutex mutex;
		std::lock_guard<std::mutex> lock(mutex);

		if (!m_pInstance) {
			// Create the actual instance of device manager only once
			m_pInstance = new AudioDeviceManagerSingleton;
			auto* mgr = &m_pInstance->audioDeviceManager;

			// Get preferred audio device name and type (if any)
			auto selected_device = juce::String(
				Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME);
			auto selected_type = juce::String(
				Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE);

			if (selected_type.isEmpty() && !selected_device.isEmpty()) {
				// Look up type for the selected device
				for (const auto t : mgr->getAvailableDeviceTypes()) {
					for (const auto n : t->getDeviceNames()) {
						if (selected_device.trim().equalsIgnoreCase(n.trim())) {
							selected_type = t->getTypeName();
							break;
						}
					}
					if(!selected_type.isEmpty())
						break;
				}
			}

			if (!selected_type.isEmpty()) {
				m_pInstance->audioDeviceManager.setCurrentAudioDeviceType(selected_type, true);
			}

			// Settings for audio device playback
			AudioDeviceManager::AudioDeviceSetup deviceSetup = AudioDeviceManager::AudioDeviceSetup();
			deviceSetup.inputChannels = 0;
			deviceSetup.outputChannels = channels;

			int initial_rate = rate;
			std::cout << "INITIAL RATE: " << initial_rate << std::endl;

			// Loop through common sample rates, starting with the user's requested rate
			// Not all sample rates are supported by audio devices, for example, many VMs
			// do not support 48000 causing no audio device to be found.

			int possible_rates[] { initial_rate, 48000, 44100, 22050 };
			for(int attempt_rate : possible_rates) {
				std::cout << "testing rate: " << attempt_rate << std::endl;

				// Resets everything to a default device setup
				m_pInstance->audioDeviceManager.initialiseWithDefaultDevices(0, channels);

				// Update the audio device setup for the current sample rate
				m_pInstance->defaultSampleRate = attempt_rate;
				deviceSetup.sampleRate = attempt_rate;
				m_pInstance->audioDeviceManager.setAudioDeviceSetup(deviceSetup, false);

				// Open the audio device with specific sample rate (if possible)
				// Not all sample rates are supported by audio devices
				juce::String audio_error = m_pInstance->audioDeviceManager.initialise(
						0,	   // number of input channels
						channels,	   // number of output channels
						nullptr, // no XML settings..
						true,	// select default device on failure
						selected_device, // preferredDefaultDeviceName
						&deviceSetup // sample_rate & channels
				);

				// Persist any errors detected
				if (audio_error.isNotEmpty()) {
					std::cout << "audio_error: " << audio_error.toStdString() << std::endl;
					m_pInstance->initialise_error = audio_error.toStdString();
				} else {
					m_pInstance->initialise_error = "";
				}

				// Determine if audio device was opened successfully, and matches the attempted sample rate
				// If all rates fail to match, a default audio device and sample rate will be opened if possible
				AudioIODevice *currentDevice = m_pInstance->audioDeviceManager.getCurrentAudioDevice();
				if (currentDevice && currentDevice->getCurrentSampleRate() == attempt_rate) {
					std::cout << "SUCCESS: rate: " << currentDevice->getCurrentSampleRate() << std::endl;
					break;
				}
			}
		}
		return m_pInstance;
	}

	// Close audio device
	void AudioDeviceManagerSingleton::CloseAudioDevice()
	{
		// Close Audio Device
		audioDeviceManager.closeAudioDevice();
		audioDeviceManager.removeAllChangeListeners();
		audioDeviceManager.dispatchPendingMessages();
	}

	// Constructor
	AudioPlaybackThread::AudioPlaybackThread(openshot::VideoCacheThread* cache)
	: juce::Thread("audio-playback")
	, player()
	, transport()
	, mixer()
	, source(NULL)
	, sampleRate(0.0)
	, numChannels(0)
	, is_playing(false)
	, time_thread("audio-buffer")
	, videoCache(cache)
	{
	}

	// Destructor
	AudioPlaybackThread::~AudioPlaybackThread()
	{
	}

	// Set the reader object
	void AudioPlaybackThread::Reader(openshot::ReaderBase *reader) {
		if (source)
			source->Reader(reader);
		else {
			// Create new audio source reader
			auto starting_frame = 1;
			source = new AudioReaderSource(reader, starting_frame);
		}

		// Set local vars
		std::cout << "AudioPlaybackThread sample rate: " << reader->info.sample_rate << std::endl;
		sampleRate = reader->info.sample_rate;
		numChannels = reader->info.channels;

		// Set video cache thread
		source->setVideoCache(videoCache);

		// Mark as 'playing'
		Play();
	}

	// Get the current frame object (which is filling the buffer)
	std::shared_ptr<openshot::Frame> AudioPlaybackThread::getFrame()
	{
	if (source) return source->getFrame();
		return std::shared_ptr<openshot::Frame>();
	}

	// Seek the audio thread
	void AudioPlaybackThread::Seek(int64_t new_position)
	{
		if (source) {
			source->Seek(new_position);
		}
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
				// Start new audio device (or get existing one)
				AudioDeviceManagerSingleton *audioInstance = 
						AudioDeviceManagerSingleton::Instance(sampleRate, numChannels);

				// Add callback
				audioInstance->audioDeviceManager.addAudioCallback(&player);

				// Create TimeSliceThread for audio buffering
				time_thread.startThread();

				// Connect source to transport
				transport.setSource(
					source,
					0, // No read ahead buffer
					&time_thread,
					0, // Sample rate correction (none)
					numChannels); // max channels
				transport.setPosition(0);
				transport.setGain(1.0);

				// Connect transport to mixer and player
				mixer.addInputSource(&transport, false);
				player.setSource(&mixer);

				// Start the transport
				transport.start();

				while (!threadShouldExit() && transport.isPlaying() && is_playing)
					std::this_thread::sleep_for(std::chrono::milliseconds(2));

				// Stop audio and shutdown transport
				Stop();
				transport.stop();

				// Kill previous audio
				transport.setSource(NULL);

				player.setSource(NULL);
				audioInstance->audioDeviceManager.removeAudioCallback(&player);

				// Remove source
				delete source;
				source = NULL;

				// Stop time slice thread
				time_thread.stopThread(-1);
			}
		}

	}
}
