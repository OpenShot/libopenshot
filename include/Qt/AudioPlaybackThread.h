/**
 * @file
 * @brief Source file for AudioPlaybackThread class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#ifndef OPENSHOT_AUDIO_PLAYBACK_THREAD_H
#define OPENSHOT_AUDIO_PLAYBACK_THREAD_H

#include "../ReaderBase.h"
#include "../RendererBase.h"
#include "../AudioReaderSource.h"
#include "../AudioDeviceInfo.h"
#include "../Settings.h"

namespace openshot
{
	struct SafeTimeSliceThread : juce::TimeSliceThread
	{
		SafeTimeSliceThread(const String & s) : juce::TimeSliceThread(s) {}
		void run()
		{
			try {
				juce::TimeSliceThread::run();
			} catch (const TooManySeeks & e) {
				// ...
			}
		}
	};

	/**
	 *  @brief Singleton wrapper for AudioDeviceManager (to prevent multiple instances).
	 */
	class AudioDeviceManagerSingleton {
	private:
		/// Default constructor (Don't allow user to create an instance of this singleton)
		AudioDeviceManagerSingleton(){ initialise_error=""; };

		/// Private variable to keep track of singleton instance
		static AudioDeviceManagerSingleton * m_pInstance;

	public:
		/// Error found during JUCE initialise method
		std::string initialise_error;

		/// List of valid audio device names
		std::vector<openshot::AudioDeviceInfo> audio_device_names;

		/// Override with no channels and no preferred audio device
		static AudioDeviceManagerSingleton * Instance();

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
		juce::WaitableEvent played;
		int buffer_size;
		bool is_playing;
		SafeTimeSliceThread time_thread;

		/// Constructor
		AudioPlaybackThread();
		/// Destructor
		~AudioPlaybackThread();

		/// Set the current thread's reader
		void Reader(openshot::ReaderBase *reader);

		/// Get the current frame object (which is filling the buffer)
		std::shared_ptr<openshot::Frame> getFrame();

		/// Get the current frame number being played
		int64_t getCurrentFramePosition();

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

        /// Get Audio Error (if any)
        std::string getError()
        {
            return AudioDeviceManagerSingleton::Instance()->initialise_error;
        }

        /// Get Audio Device Names (if any)
        std::vector<openshot::AudioDeviceInfo> getAudioDeviceNames()
        {
            return AudioDeviceManagerSingleton::Instance()->audio_device_names;
        };

		friend class PlayerPrivate;
		friend class QtPlayer;
    };

}

#endif // OPENSHOT_AUDIO_PLAYBACK_THREAD_H
