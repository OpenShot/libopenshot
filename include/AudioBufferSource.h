#ifndef OPENSHOT_AUDIOBUFFERSOURCE_H
#define OPENSHOT_AUDIOBUFFERSOURCE_H

/**
 * \file
 * \brief Header file for AudioBufferSource class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

/// Do not include the juce unittest headers, because it collides with unittest++
#define __JUCE_UNITTEST_JUCEHEADER__

#include <iomanip>
#include "JuceLibraryCode/JuceHeader.h"

using namespace std;

namespace openshot
{

	/**
	 * \brief This class is used to expose an AudioSampleBuffer as an AudioSource in juce.
	 *
	 * The juce library cannot play audio directly from an AudioSampleBuffer, so this class exposes
	 * an AudioSampleBuffer as a AudioSource, so that juce can play the audio.
	 */
	class AudioBufferSource : public PositionableAudioSource
	{
	private:
		int position;
		int start;
		bool repeat;
		AudioSampleBuffer *buffer;

	public:
		/// Default constructor
		AudioBufferSource(int samples, int channels);

		/// Destructor
		~AudioBufferSource();

		/// Get the next block of audio samples
		void getNextAudioBlock (const AudioSourceChannelInfo& info);

		/// Prepare to play this audio source
		void prepareToPlay(int, double);

		/// Release all resources
		void releaseResources();

		/// Set the next read position of this source
		void setNextReadPosition (long long newPosition);

		/// Get the next read position of this source
		long long getNextReadPosition() const;

		/// Get the total length (in samples) of this audio source
		long long getTotalLength() const;

		/// Determines if this audio source should repeat when it reaches the end
		bool isLooping() const;

		/// Set if this audio source should repeat when it reaches the end
		void setLooping (bool shouldLoop);

		/// Add audio samples to a specific channel
		void AddAudio(int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource);
	};

}

#endif
