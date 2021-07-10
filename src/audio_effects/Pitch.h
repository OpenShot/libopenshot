/**
 * @file
 * @brief Header file for Pitch audio effect class
 * @author 
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

#ifndef OPENSHOT_PITCH_AUDIO_EFFECT_H
#define OPENSHOT_PITCH_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "../Enums.h"

#include <memory>
#include <string>
#include <math.h>
#include <cmath>


namespace openshot
{

	/**
	 * @brief This class adds a pitch into the audio
	 *
	 */
	class Pitch : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		Keyframe shift;	///< Pitch shift keyframe. The pitch shift inserted on the audio.
		openshot::FFTSize fft_size;
		openshot::HopSize hop_size;
		openshot::WindowType window_type;

		/// Blank constructor, useful when using Json to load the effect properties
		Pitch();

		/// Default constructor
		///
		/// @param new_level The audio default pitch level (between 1 and 100)
		Pitch(Keyframe new_shift, openshot::FFTSize new_fft_size, openshot::HopSize new_hop_size, openshot::WindowType new_window_type);

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// new openshot::Frame object. All Clip keyframes and effects are resolved into
		/// pixels.
		///
		/// @returns A new openshot::Frame object
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override { 
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number); 
		}

		/// @brief This method is required for all derived classes of ClipBase, and returns a
		/// modified openshot::Frame object
		///
		/// The frame object is passed into this method and used as a starting point (pixels and audio).
		/// All Clip keyframes and effects are resolved into pixels.
		///
		/// @returns The modified openshot::Frame object
		/// @param frame The frame object that needs the clip or effect applied to it
		/// @param frame_number The frame number (starting at 1) of the clip or effect on the timeline.
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Get all properties for a specific frame (perfect for a UI to display the current state
		/// of all properties at any time)
		std::string PropertiesJSON(int64_t requested_frame) const override;


		void updateFftSize(std::shared_ptr<openshot::Frame> frame);
		void updateHopSize();
		void updateAnalysisWindow();
		void updateWindow(const juce::HeapBlock<float>& window, const int window_length);
		void updateWindowScaleFactor();
		float princArg(const float phase);


		juce::CriticalSection lock;
		std::unique_ptr<juce::dsp::FFT> fft;

		int input_buffer_length;
		int input_buffer_write_position;
		juce::AudioSampleBuffer input_buffer;

		int output_buffer_length;
		int output_buffer_write_position;
		int output_buffer_read_position;
		juce::AudioSampleBuffer output_buffer;

		juce::HeapBlock<float> fft_window;
		juce::HeapBlock<juce::dsp::Complex<float>> fft_time_domain;
		juce::HeapBlock<juce::dsp::Complex<float>> fft_frequency_domain;

		int samples_since_last_FFT;

		int overlap;
		int hopSize;
		float window_scale_factor;

		juce::HeapBlock<float> omega;
		juce::AudioSampleBuffer input_phase;
		juce::AudioSampleBuffer output_phase;
		bool need_to_reset_phases;
	};

}

#endif
