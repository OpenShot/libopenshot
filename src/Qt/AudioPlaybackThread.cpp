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
#include "AudioDeviceInitialization.h"
#include "Settings.h"

#include "../ReaderBase.h"
#include "../RendererBase.h"
#include "../AudioReaderSource.h"
#include "../AudioDevices.h"
#include "../Settings.h"
#include "../Logger.h"

#include <mutex>
#include <thread>	// for std::this_thread::sleep_for
#include <chrono>	// for std::chrono::milliseconds
#include <sstream>
#include <condition_variable>
#include <mutex>

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
			auto result = detail::InitializeAudioDevice(m_pInstance->audioDeviceManager,
				{Settings::Instance()->PLAYBACK_AUDIO_DEVICE_TYPE,
				 Settings::Instance()->PLAYBACK_AUDIO_DEVICE_NAME},
				rate, channels, Settings::Instance()->PLAYBACK_AUDIO_BUFFER_SIZE);
			m_pInstance->initialise_error = result.error;
			m_pInstance->currentAudioDevice = result.device;
			m_pInstance->defaultSampleRate = result.sample_rate;
			Logger::Instance()->AppendDebugMethod("AudioDeviceManagerSingleton::Instance (audio device initialization completed)");
		}
		return m_pInstance;
	}

	detail::AudioDeviceResult detail::InitializeAudioDevice(juce::AudioDeviceManager& manager,
		AudioDeviceInfo requested_device, int rate, int channels, int buffer_size)
	{
		AudioDeviceResult result;
		AudioIODevice *foundAudioIODevice = NULL;

		std::stringstream constructor_title;
		constructor_title << "AudioDeviceManagerSingleton::Instance (default audio device type: " <<
			requested_device.type << ", default audio device name: " <<
			requested_device.name << ")";
		Logger::Instance()->AppendDebugMethod(constructor_title.str(), "channels", channels, "buffer", buffer_size);

		// Find missing device type (if needed)
		if (requested_device.type.isEmpty() && !requested_device.name.isEmpty()) {
			for (const auto t : manager.getAvailableDeviceTypes()) {
				t->scanForDevices();
				for (const auto n : t->getDeviceNames()) {
					if (requested_device.name.trim().equalsIgnoreCase(n.trim())) {
						requested_device.type = t->getTypeName();
						break;
					}
				}
			}
		}

		// Populate all possible device types and device names (starting with the user's requested settings)
		std::vector<openshot::AudioDeviceInfo> devices{ { requested_device } };
		for (const auto t : manager.getAvailableDeviceTypes()) {
			std::stringstream type_debug;
			type_debug << "AudioDeviceManagerSingleton::Instance (iterate audio device type: " <<  t->getTypeName() << ")";
			Logger::Instance()->AppendDebugMethod(type_debug.str(), "rate", rate, "channels", channels);

			t->scanForDevices();
			for (const auto n : t->getDeviceNames()) {
				AudioDeviceInfo device = { t->getTypeName(), n.trim() };
				devices.push_back(device);
				std::stringstream device_debug;
				device_debug << "AudioDeviceManagerSingleton::Instance (iterate audio device name: " <<  device.name << ", type: " <<  t->getTypeName() << ")";
				Logger::Instance()->AppendDebugMethod(device_debug.str(), "rate", rate, "channels", channels);
			}
		}

		// Loop through all device combinations (starting with the requested one)
		for (auto attempt_device : devices) {

			// Resets everything to a default device setup
			manager.initialiseWithDefaultDevices(0, channels);

			// Set device type (if any)
			if (!attempt_device.type.isEmpty()) {
				manager.setCurrentAudioDeviceType(attempt_device.type, true);
			}

			// JUCE ignores preferredDefaultDeviceName when a setup is supplied, so
			// the requested output must be included in the setup itself.
			AudioDeviceManager::AudioDeviceSetup deviceSetup = AudioDeviceManager::AudioDeviceSetup();
			deviceSetup.outputDeviceName = attempt_device.name;
			deviceSetup.inputChannels = 0;
			deviceSetup.outputChannels = channels;
			deviceSetup.bufferSize = buffer_size;

			// Loop through common sample rates, starting with the user's requested rate
			// Not all sample rates are supported by audio devices, for example, many VMs
			// do not support 48000 causing no audio device to be found.
			int possible_rates[] { rate, 48000, 44100, 22050 };
			for(int attempt_rate : possible_rates) {
				std::stringstream title_rate;
				title_rate << "AudioDeviceManagerSingleton::Instance (attempt audio device name: " <<  attempt_device.name << ")";
				Logger::Instance()->AppendDebugMethod(title_rate.str(), "rate", attempt_rate, "channels", channels);

				// Pass the setup to initialise once; do not open it separately first.
				deviceSetup.sampleRate = attempt_rate;

				// Open the audio device with specific sample rate (if possible)
				// Not all sample rates are supported by audio devices
				juce::String audio_error = manager.initialise(
						0,		 // number of input channels
						channels,					 // number of output channels
						nullptr,		   // no XML settings..
						true, // select default device on failure
						attempt_device.name,  // preferredDefaultDeviceName
						&deviceSetup				 // sample_rate & channels
				);

				// Persist any errors detected
				result.error = audio_error.toStdString();

				if (!result.error.empty()) {
					std::stringstream title_error;
					title_error << "AudioDeviceManagerSingleton::Instance (audio device error: " <<
					result.error << ")";
					Logger::Instance()->AppendDebugMethod(title_error.str(), "rate", attempt_rate, "channels", channels);
				}

				// Determine if audio device was opened successfully, and matches the attempted sample rate
				// If all rates fail to match, a default audio device and sample rate will be opened if possible
				foundAudioIODevice = manager.getCurrentAudioDevice();
				if (audio_error.isEmpty() && foundAudioIODevice && foundAudioIODevice->isOpen()
					&& !foundAudioIODevice->getActiveOutputChannels().isZero()) {
					result.device = {foundAudioIODevice->getTypeName(), foundAudioIODevice->getName()};
					result.sample_rate = foundAudioIODevice->getCurrentSampleRate();
					result.error.clear();
					if (result.sample_rate != attempt_rate)
						continue;
					// Successfully tested a sample rate
					std::stringstream title_found;
					title_found << "AudioDeviceManagerSingleton::Instance (successful audio device found: " <<
					foundAudioIODevice->getTypeName() << ", name: " << foundAudioIODevice->getName() << ")";
					Logger::Instance()->AppendDebugMethod(title_found.str(), "rate", attempt_rate, "channels", channels);
					break;
				}
				result.device = {};
				result.sample_rate = 0.0;
				if (result.error.empty()) result.error = "no channels";
			}

			if (result.sample_rate > 0.0) {
				// Successfully opened an audio device
				break;
			}
		}
		return result;
	}

	// Close audio device
	void AudioDeviceManagerSingleton::CloseAudioDevice()
	{
		// Close Audio Device
		audioDeviceManager.closeAudioDevice();
		audioDeviceManager.removeAllChangeListeners();
		audioDeviceManager.dispatchPendingMessages();

		delete m_pInstance;
		m_pInstance = NULL;
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
        // Detach the JUCE callback in run() before releasing its source.
        signalThreadShouldExit();
        NotifyTransportStateChanged();
        stopThread(-1);
        delete source;
	}

	// Set the reader object
	void AudioPlaybackThread::Reader(openshot::ReaderBase *reader) {
		if (source) {
            // Caller must stop/join playback before replacing the borrowed reader.
            source->Reader(reader);
            source->Seek(1);
        }
		else {
			// Create new audio source reader
			auto starting_frame = 1;
			source = new AudioReaderSource(reader, starting_frame);
		}

		// Set local vars
		sampleRate = reader->info.sample_rate;
		numChannels = reader->info.channels;

        Logger::Instance()->AppendDebugMethod("AudioPlaybackThread::Reader", "rate", sampleRate, "channel", numChannels);

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

	// Override Play and Stop to notify of state changes
	void AudioPlaybackThread::Play() {
		is_playing = true;
		NotifyTransportStateChanged();
	}

	void AudioPlaybackThread::Stop() {
		is_playing = false;
        if (source) source->Seek(1);
		NotifyTransportStateChanged();
	}

	void AudioPlaybackThread::NotifyTransportStateChanged()
	{
		std::lock_guard<std::mutex> lock(transportMutex);
		transportCondition.notify_all();
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
				time_thread.startThread(Priority::high);

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

                    while (!threadShouldExit() && transport.isPlaying() && is_playing) {
                        // Wait until transport state changes or thread should exit
                        std::unique_lock<std::mutex> lock(transportMutex);
                        transportCondition.wait_for(lock, std::chrono::milliseconds(10), [this]() {
                            return threadShouldExit() || !transport.isPlaying() || !is_playing;
                        });
                    }

				// Stop audio and shutdown transport
				Stop();
				transport.stop();

				// Kill previous audio
				transport.setSource(NULL);

				player.setSource(NULL);
				audioInstance->audioDeviceManager.removeAudioCallback(&player);

				// Keep the source alive across Stop/Play. Control callers can
                // enqueue a seek without racing source destruction. The worker
                // detaches it above; the owner deletes it after joining.
                mixer.removeInputSource(&transport);

				// Stop time slice thread
				time_thread.stopThread(-1);
			} else {
                std::unique_lock<std::mutex> lock(transportMutex);
                transportCondition.wait_for(lock, std::chrono::milliseconds(10), [this] {
                    return threadShouldExit() || (source && is_playing.load());
                });
            }
		}

	}
}
