/**
 * @file
 * @brief Header file for AudioBufferSource class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_AUDIOBUFFERSOURCE_H
#define OPENSHOT_AUDIOBUFFERSOURCE_H

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace openshot
{
	/**
	 * @brief This class is used to expose an AudioBuffer<float> as an AudioSource in JUCE.
	 *
	 * The <a href="http://www.juce.com/">JUCE</a> library cannot play audio directly from an AudioBuffer<float>, so this class exposes
	 * an AudioBuffer<float> as a AudioSource, so that JUCE can play the audio.
	 */
	class AudioBufferSource : public juce::PositionableAudioSource
	{
	private:
		int position;
		bool repeat;
		juce::AudioBuffer<float> *buffer;

	public:
		/// @brief Default constructor
		/// @param audio_buffer This buffer contains the samples you want to play through JUCE.
		AudioBufferSource(juce::AudioBuffer<float> *audio_buffer);

		/// Destructor
		~AudioBufferSource();

		/// @brief Get the next block of audio samples
		/// @param info This struct informs us of which samples are needed next.
		void getNextAudioBlock (const juce::AudioSourceChannelInfo& info);

		/// Prepare to play this audio source
		void prepareToPlay(int, double);

		/// Release all resources
		void releaseResources();

		/// @brief Set the next read position of this source
		/// @param newPosition The sample # to start reading from
		void setNextReadPosition (juce::int64 newPosition);

		/// Get the next read position of this source
		juce::int64 getNextReadPosition() const;

		/// Get the total length (in samples) of this audio source
		juce::int64 getTotalLength() const;

		/// Determines if this audio source should repeat when it reaches the end
		bool isLooping() const;

		/// @brief Set if this audio source should repeat when it reaches the end
		/// @param shouldLoop Determines if the audio source should repeat when it reaches the end
		void setLooping (bool shouldLoop);

		/// Update the internal buffer used by this source
		void setBuffer (juce::AudioBuffer<float> *audio_buffer);
	};

}

#endif
