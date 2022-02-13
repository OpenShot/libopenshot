/**
 * @file
 * @brief Header file for AudioReaderSource class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIOREADERSOURCE_H
#define OPENSHOT_AUDIOREADERSOURCE_H

#include "ReaderBase.h"
#include "Qt/VideoCacheThread.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

/// This namespace is the default namespace for all code in the openshot library
namespace openshot
{
	class Frame;
	/**
	 * @brief This class is used to expose any ReaderBase derived class as an AudioSource in JUCE.
	 *
	 * This allows any reader to play audio through JUCE (our audio framework).
	 */
	class AudioReaderSource : public juce::PositionableAudioSource
	{
	private:
	    int stream_position; /// The absolute stream position (required by PositionableAudioSource, but ignored)
		int frame_position; /// The frame position (current frame for audio playback)
		int speed;          /// The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)

		ReaderBase *reader; /// The reader to pull samples from
		std::shared_ptr<Frame> frame; /// The current frame object that is being read
		int64_t sample_position; /// The position of the current frame's audio buffer
        openshot::VideoCacheThread *videoCache; /// The cache thread (for pre-roll checking)

	public:

		/// @brief Constructor that reads samples from a reader
		/// @param audio_reader This reader provides constant samples from a ReaderBase derived class
		/// @param starting_frame_number This is the frame number to start reading samples from the reader.
		AudioReaderSource(ReaderBase *audio_reader, int64_t starting_frame_number);

		/// Destructor
		~AudioReaderSource();

		/// @brief Get the next block of audio samples
		/// @param info This struct informs us of which samples are needed next.
		void getNextAudioBlock (const juce::AudioSourceChannelInfo& info);

		/// Prepare to play this audio source
		void prepareToPlay(int, double);

		/// Release all resources
		void releaseResources();

		/// @brief Set the next read position of this source
		/// @param newPosition The sample # to start reading from
		void setNextReadPosition (juce::int64 newPosition) { stream_position = newPosition; };

		/// Get the next read position of this source
		juce::int64 getNextReadPosition() const { return stream_position; };

		/// Get the total length (in samples) of this audio source
		juce::int64 getTotalLength() const;

		/// Looping is not support in OpenShot audio playback (this is always false)
		bool isLooping() const { return false; };

		/// @brief This method is ignored (we do not support looping audio playback)
		/// @param shouldLoop Determines if the audio source should repeat when it reaches the end
		void setLooping (bool shouldLoop) {  };

	    /// Return the current frame object
	    std::shared_ptr<Frame> getFrame() const { return frame; }

	    /// Set Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	    void setSpeed(int new_speed) { speed = new_speed; }
	    /// Get Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	    int getSpeed() const { return speed; }

	    /// Set playback video cache thread (for pre-roll reference)
	    void setVideoCache(openshot::VideoCacheThread *newCache) { videoCache = newCache; }

	    /// Set Reader
	    void Reader(ReaderBase *audio_reader) { reader = audio_reader; }
	    /// Get Reader
	    ReaderBase* Reader() const { return reader; }

	    /// Seek to a specific frame
	    void Seek(int64_t new_position) { frame_position = new_position; sample_position=0; }

	};

}

#endif
