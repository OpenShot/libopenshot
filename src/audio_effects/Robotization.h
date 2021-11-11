/**
 * @file
 * @brief Header file for Robotization audio effect class
 * @author
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_ROBOTIZATION_AUDIO_EFFECT_H
#define OPENSHOT_ROBOTIZATION_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

#include <memory>
#include <mutex>
#include <string>

#include "EffectBase.h"

#include "STFT.h"

#include "Json.h"
#include "KeyFrame.h"
#include "Enums.h"

namespace juce {
    namespace dsp {
        class FFT;
    }
}
namespace openshot
{
	class Frame;

	/**
	 * @brief This class adds a robotization effect into the audio
	 *
	 */
	class Robotization : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		openshot::FFTSize fft_size;
		openshot::HopSize hop_size;
		openshot::WindowType window_type;

		/// Default constructor
		Robotization();

		/// Constructor
		Robotization(openshot::FFTSize fft_size,
		             openshot::HopSize hop_size,
		             openshot::WindowType window_type);

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}

		std::shared_ptr<openshot::Frame>
		GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		std::string PropertiesJSON(int64_t requested_frame) const override;


		class RobotizationEffect : public STFT
		{
		public:
			RobotizationEffect (Robotization& p) : parent (p) { }

		private:
			void modification(const int channel) override;

			Robotization &parent;
		};

		std::recursive_mutex mutex;
    	RobotizationEffect stft;
		std::unique_ptr<juce::dsp::FFT> fft;
	};

}  // namespace

#endif  // OPENSHOT_ROBOTIZATION_AUDIO_EFFECT_H
