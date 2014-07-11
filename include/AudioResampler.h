/**
 * @file
 * @brief Header file for AudioResampler class
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

#ifndef OPENSHOT_RESAMPLER_H
#define OPENSHOT_RESAMPLER_H

/// Do not include the juce unittest headers, because it collides with unittest++
#ifndef __JUCE_UNITTEST_JUCEHEADER__
	#define __JUCE_UNITTEST_JUCEHEADER__
#endif

#ifndef _NDEBUG
	// Define NO debug for JUCE on mac os
	#define _NDEBUG
#endif

#include "JuceLibraryCode/JuceHeader.h"
#include "AudioBufferSource.h"
#include "Exceptions.h"

namespace openshot {

	/**
	 * @brief This class is used to resample audio data for many sequential frames.
	 *
	 * It maintains some data from the last call to GetResampledBuffer(), so there
	 * are no pops and clicks between frames.
	 */
	class AudioResampler {
	private:
		AudioSampleBuffer *buffer;
		AudioSampleBuffer *resampled_buffer;
		AudioBufferSource *buffer_source;
		ResamplingAudioSource *resample_source;
		AudioSourceChannelInfo resample_callback_buffer;

		int num_of_samples;
		int new_num_of_samples;
		double dest_ratio;
		double source_ratio;
		bool isPrepared;

	public:
		/// Default constructor
		AudioResampler();

		/// Destructor
		~AudioResampler();

		/// @brief Sets the audio buffer and key settings
		/// @param new_buffer The buffer of audio samples needing to be resampled
		/// @param sample_rate The original sample rate of the buffered samples
		/// @param new_sample_rate The requested sample rate you need
		void SetBuffer(AudioSampleBuffer *new_buffer, double sample_rate, double new_sample_rate);

		/// @brief Sets the audio buffer and key settings
		/// @param new_buffer The buffer of audio samples needing to be resampled
		/// @param ratio The multiplier that needs to be applied to the sample rate (this is how resampling happens)
		void SetBuffer(AudioSampleBuffer *new_buffer, double ratio);

		/// Get the resampled audio buffer
		AudioSampleBuffer* GetResampledBuffer();
	};

}

#endif
