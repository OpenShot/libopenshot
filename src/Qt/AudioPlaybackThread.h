/**
 * @file
 * @brief Source file for AudioPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIO_PLAYBACK_THREAD_H
#define OPENSHOT_AUDIO_PLAYBACK_THREAD_H

#include "ReaderBase.h"
#include "RendererBase.h"
#include "AudioReaderSource.h"
#include "AudioDevices.h"
#include "AudioReaderSource.h"
#include "Qt/VideoCacheThread.h"

#include <OpenShotAudio.h>
#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace openshot
{

// Forward decls
class ReaderBase;
class Frame;
class PlayerPrivate;
class QtPlayer;


/**
 *  @brief Singleton wrapper for AudioDeviceManager (to prevent multiple instances).
 */
class AudioDeviceManagerSingleton {
private:
		/// Default constructor (Don't allow user to create an instance of this singleton)
		AudioDeviceManagerSingleton(){ initialise_error=""; };

		/// Private variable to keep track of singleton instance
		static AudioDeviceManagerSingleton* m_pInstance;

public:
		/// Error found during JUCE initialise method
		std::string initialise_error;

		/// Default sample rate (as detected)
		double defaultSampleRate;

		/// Current open audio device (or last attempted device - if none were successful)
		AudioDeviceInfo currentAudioDevice;

		/// Override with default sample rate & channels (44100, 2) and no preferred audio device
		static AudioDeviceManagerSingleton* Instance();

		/// Override with custom sample rate & channels and no preferred audio device
		/// sample rate and channels are only set on 1st call (when singleton is created)
		static AudioDeviceManagerSingleton* Instance(int rate, int channels);

		/// Public device manager property
		juce::AudioDeviceManager audioDeviceManager;

		/// Close audio device
		void CloseAudioDevice();
	};

	/**
	 *  @brief The audio playback thread
	 */
	class AudioPlaybackThread : juce::Thread
	{
		juce::AudioSourcePlayer player;
		juce::AudioTransportSource transport;
		juce::MixerAudioSource mixer;
		AudioReaderSource *source;
		double sampleRate;
		int numChannels;
		juce::WaitableEvent play;
		bool is_playing;
		juce::TimeSliceThread time_thread;
		openshot::VideoCacheThread *videoCache; /// The cache thread (for pre-roll checking)

		/// Constructor
		AudioPlaybackThread(openshot::VideoCacheThread* cache);
		/// Destructor
		~AudioPlaybackThread();

		/// Set the current thread's reader
		void Reader(openshot::ReaderBase *reader);

		/// Get the current frame object (which is filling the buffer)
		std::shared_ptr<openshot::Frame> getFrame();

		/// Play the audio
		void Play();

		/// Seek the audio thread
		void Seek(int64_t new_position);

		/// Stop the audio playback
		void Stop();

		/// Start thread
		void run();

		/// Set Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
		void setSpeed(int new_speed) { if (source) source->setSpeed(new_speed); }

		/// Get Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
		int getSpeed() const { if (source) return source->getSpeed(); else return 1; }

		/// Get audio initialization errors (if any)
		std::string getError()
		{
			return AudioDeviceManagerSingleton::Instance()->initialise_error;
		}

		/// Get detected audio sample rate (from active, open audio device)
		double getDefaultSampleRate()
		{
			return AudioDeviceManagerSingleton::Instance()->defaultSampleRate;
		}

		/// Get active, open audio device (or last attempted device if none were successful)
		AudioDeviceInfo getCurrentAudioDevice()
		{
			return AudioDeviceManagerSingleton::Instance()->currentAudioDevice;
		}

		/// Get vector of audio device names & types
		AudioDeviceList getAudioDeviceNames()
		{
			AudioDevices devs;
			return devs.getNames();
		};

		friend class PlayerPrivate;
		friend class QtPlayer;
};

}  // namespace openshot

#endif // OPENSHOT_AUDIO_PLAYBACK_THREAD_H
