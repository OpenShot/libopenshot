/**
 * @file
 * @brief Header file for whisperization audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_WHISPERIZATION_AUDIO_EFFECT_H
#define OPENSHOT_WHISPERIZATION_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

#include <memory>
#include <mutex>
#include <string>

#include "../EffectBase.h"

#include "../Json.h"
#include "../KeyFrame.h"
#include "../Enums.h"
#include "STFT.h"

namespace juce {
    namespace dsp {
        class FFT;
    }
}

namespace openshot
{
    class Frame;
	/**
	 * @brief This class adds a whisperization effect into the audio
	 *
	 */
	class Whisperization : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		openshot::FFTSize fft_size;
		openshot::HopSize hop_size;
		openshot::WindowType window_type;

		/// Blank constructor, useful when using Json to load the effect properties
		Whisperization();

		/// Default constructor
		///
		/// @param new_level The audio default Whisperization level (between 1 and 100)
		Whisperization(openshot::FFTSize new_fft_size, openshot::HopSize new_hop_size, openshot::WindowType new_window_type);

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


		class WhisperizationEffect : public STFT
		{
		public:
			WhisperizationEffect(Whisperization& p) : parent (p) { }

		private:
			void modification(const int channel) override;

			Whisperization &parent;
		};

		std::recursive_mutex mutex;
    	WhisperizationEffect stft;
		std::unique_ptr<juce::dsp::FFT> fft;
	};

}

#endif
