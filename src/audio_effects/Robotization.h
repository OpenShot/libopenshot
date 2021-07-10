/**
 * @file
 * @brief Header file for Robotization audio effect class
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

#ifndef OPENSHOT_ROBOTIZATION_AUDIO_EFFECT_H
#define OPENSHOT_ROBOTIZATION_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "../Enums.h"
#include "STFT.h"

#include <memory>
#include <string>
#include <math.h>
#include <cmath>


namespace openshot
{

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
		// Keyframe shift;	///< Robotization shift keyframe. The Robotization shift inserted on the audio.
		openshot::FFTSize fft_size;
		openshot::HopSize hop_size;
		openshot::WindowType window_type;
		openshot::RobotizationEffectType effect_type;

		/// Blank constructor, useful when using Json to load the effect properties
		Robotization();

		/// Default constructor
		///
		/// @param new_level The audio default Robotization level (between 1 and 100)
		Robotization(openshot::FFTSize new_fft_size, openshot::HopSize new_hop_size, openshot::WindowType new_window_type, openshot::RobotizationEffectType new_effect_type);

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


		class RobotizationWhisperizationEffect : public STFT
		{
		public:
			RobotizationWhisperizationEffect (Robotization& p) : parent (p) { }

		private:
			void modification() override
			{
				fft->perform(time_domain_buffer, frequency_domain_buffer, false);

				switch ((int)parent.effect_type) {
					case PASS_THROUGH: {
						// nothing
						break;
					}
					case ROBOTIZATION: {
						for (int index = 0; index < fft_size; ++index) {
							float magnitude = abs(frequency_domain_buffer[index]);
							frequency_domain_buffer[index].real(magnitude);
							frequency_domain_buffer[index].imag(0.0f);
						}
						break;
					}
					case WHISPERIZATION: {
						for (int index = 0; index < fft_size / 2 + 1; ++index) {
							float magnitude = abs(frequency_domain_buffer[index]);
							float phase = 2.0f * M_PI * (float)rand() / (float)RAND_MAX;

							frequency_domain_buffer[index].real(magnitude * cosf (phase));
							frequency_domain_buffer[index].imag(magnitude * sinf (phase));
							if (index > 0 && index < fft_size / 2) {
								frequency_domain_buffer[fft_size - index].real (magnitude * cosf (phase));
								frequency_domain_buffer[fft_size - index].imag (magnitude * sinf (-phase));
							}
						}
						break;
					}
				}

				fft->perform(frequency_domain_buffer, time_domain_buffer, true);
			}

			Robotization &parent;
		};

		juce::CriticalSection lock;
    	RobotizationWhisperizationEffect stft;
		std::unique_ptr<juce::dsp::FFT> fft;
	};

}

#endif
