/**
 * @file
 * @brief Header file for AudioReaderSource class
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

#ifndef OPENSHOT_AUDIOREADERSOURCE_H
#define OPENSHOT_AUDIOREADERSOURCE_H

/// Do not include the juce unittest headers, because it collides with unittest++
#define __JUCE_UNITTEST_JUCEHEADER__

#ifndef _NDEBUG
	/// Define NO debug for JUCE on mac os
	#define _NDEBUG
#endif

#include <iomanip>
#include "JuceLibraryCode/JuceHeader.h"
#include "ReaderBase.h"

using namespace std;

/// This namespace is the default namespace for all code in the openshot library
namespace openshot
{

	/**
	 * @brief This class is used to expose any ReaderBase derived class as an AudioSource in JUCE.
	 *
	 * This allows any reader to play audio through JUCE (our audio framework).
	 */
	class AudioReaderSource : public PositionableAudioSource
	{
	private:
		int position; /// The position of the audio source (index of buffer)
		bool repeat; /// Repeat the audio source when finished
		int size; /// The size of the internal buffer
		AudioSampleBuffer *buffer; /// The audio sample buffer
		int speed; /// The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)

		ReaderBase *reader; /// The reader to pull samples from
		int64 original_frame_number; /// The current frame to read from
		int64 frame_number; /// The current frame number
		std::shared_ptr<Frame> frame; /// The current frame object that is being read
		long int frame_position; /// The position of the current frame's buffer
		double estimated_frame; /// The estimated frame position of the currently playing buffer
		int estimated_samples_per_frame; /// The estimated samples per frame of video

		/// Get more samples from the reader
		void GetMoreSamplesFromReader();

		/// Reverse an audio buffer (for backwards audio)
		juce::AudioSampleBuffer* reverse_buffer(juce::AudioSampleBuffer* buffer);

	public:

		/// @brief Constructor that reads samples from a reader
		/// @param audio_reader This reader provides constant samples from a ReaderBase derived class
		/// @param starting_frame_number This is the frame number to start reading samples from the reader.
		/// @param buffer_size The max number of samples to keep in the buffer at one time.
		AudioReaderSource(ReaderBase *audio_reader, int64 starting_frame_number, int buffer_size);

		/// Destructor
		~AudioReaderSource();

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

	    const ReaderInfo & getReaderInfo() const { return reader->info; }

	    /// Return the current frame object
	    std::shared_ptr<Frame> getFrame() const { return frame; }

	    /// Get the estimate frame that is playing at this moment
	    long int getEstimatedFrame() const { return long(estimated_frame); }

	    /// Set Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	    void setSpeed(int new_speed) { speed = new_speed; }
	    /// Get Speed (The speed and direction to playback a reader (1=normal, 2=fast, 3=faster, -1=rewind, etc...)
	    int getSpeed() const { return speed; }

	    /// Set Reader
	    void Reader(ReaderBase *audio_reader) { reader = audio_reader; }
	    /// Get Reader
	    ReaderBase* Reader() const { return reader; }

	    /// Seek to a specific frame
	    void Seek(int64 new_position) { frame_number = new_position; estimated_frame = new_position; }

	};

}

#endif
