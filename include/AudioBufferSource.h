/**
 * @file
 * @brief Header file for AudioBufferSource class
 * @author Jonathan Thomas <jonathan@openshot.org>
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

#ifndef OPENSHOT_AUDIOBUFFERSOURCE_H
#define OPENSHOT_AUDIOBUFFERSOURCE_H

/// Do not include the juce unittest headers, because it collides with unittest++
#define __JUCE_UNITTEST_JUCEHEADER__

#ifndef _NDEBUG
	/// Define NO debug for JUCE on mac os
	#define _NDEBUG
#endif

#include <iomanip>
#include "JuceLibraryCode/JuceHeader.h"

using namespace std;

/// This namespace is the default namespace for all code in the openshot library
namespace openshot
{

	/**
	 * @brief This class is used to expose an AudioSampleBuffer as an AudioSource in JUCE.
	 *
	 * The <a href="http://www.juce.com/">JUCE</a> library cannot play audio directly from an AudioSampleBuffer, so this class exposes
	 * an AudioSampleBuffer as a AudioSource, so that JUCE can play the audio.
	 */
	class AudioBufferSource : public PositionableAudioSource
	{
	private:
		int position;
		int start;
		bool repeat;
		AudioSampleBuffer *buffer;

	public:
		/// @brief Default constructor
		/// @param audio_buffer This buffer contains the samples you want to play through JUCE.
		AudioBufferSource(AudioSampleBuffer *audio_buffer);

		/// Destructor
		~AudioBufferSource();

		/// @brief Get the next block of audio samples
		/// @param info This struct informs us of which samples are needed next.
		void getNextAudioBlock (const AudioSourceChannelInfo& info);

		/// Prepare to play this audio source
		void prepareToPlay(int, double);

		/// Release all resources
		void releaseResources();

		/// @brief Set the next read position of this source
		/// @param newPosition The sample # to start reading from
		void setNextReadPosition (long long newPosition);

		/// Get the next read position of this source
		long long getNextReadPosition() const;

		/// Get the total length (in samples) of this audio source
		long long getTotalLength() const;

		/// Determines if this audio source should repeat when it reaches the end
		bool isLooping() const;

		/// @brief Set if this audio source should repeat when it reaches the end
		/// @param shouldLoop Determines if the audio source should repeat when it reaches the end
		void setLooping (bool shouldLoop);

		/// Update the internal buffer used by this source
		void setBuffer (AudioSampleBuffer *audio_buffer);
	};

}

#endif
