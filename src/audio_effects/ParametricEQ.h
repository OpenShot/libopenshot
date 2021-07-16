/**
 * @file
 * @brief Header file for Parametric EQ audio effect class
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

#ifndef OPENSHOT_PARAMETRIC_EQ_AUDIO_EFFECT_H
#define OPENSHOT_PARAMETRIC_EQ_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

#include "../EffectBase.h"

#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"
#include "../Enums.h"

#include <memory>
#include <string>
#include <math.h>
// #include <OpenShotAudio.h>


namespace openshot
{

	/**
	 * @brief This class adds a equalization into the audio
	 *
	 */
	class ParametricEQ : public EffectBase
	{
	private:
		/// Init effect settings
		void init_effect_details();

	public:
		openshot::FilterType filter_type;	
		Keyframe frequency;	
		Keyframe q_factor;
		Keyframe gain;

		/// Blank constructor, useful when using Json to load the effect properties
		ParametricEQ();

		/// Default constructor
		///
		/// @param new_level 
		ParametricEQ(openshot::FilterType new_filter_type, Keyframe new_frequency, Keyframe new_gain, Keyframe new_q_factor);

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

		class Filter : public IIRFilter
		{
		public:
			void updateCoefficients (const double discrete_frequency,
									 const double q_factor,
									 const double gain,
									 const int filter_type) noexcept
			{
				jassert (discrete_frequency > 0);
				jassert (q_factor > 0);

				double bandwidth = jmin (discrete_frequency / q_factor, M_PI * 0.99);
				double two_cos_wc = -2.0 * cos (discrete_frequency);
				double tan_half_bw = tan (bandwidth / 2.0);
				double tan_half_wc = tan (discrete_frequency / 2.0);
				double sqrt_gain = sqrt (gain);

				switch (filter_type) {
					case 0 /* LOW_PASS */: {
						coefficients = IIRCoefficients (/* b0 */ tan_half_wc,
														/* b1 */ tan_half_wc,
														/* b2 */ 0.0,
														/* a0 */ tan_half_wc + 1.0,
														/* a1 */ tan_half_wc - 1.0,
														/* a2 */ 0.0);
						break;
					}
					case 1 /* HIGH_PASS */: {
						coefficients = IIRCoefficients (/* b0 */ 1.0,
														/* b1 */ -1.0,
														/* b2 */ 0.0,
														/* a0 */ tan_half_wc + 1.0,
														/* a1 */ tan_half_wc - 1.0,
														/* a2 */ 0.0);
						break;
					}
					case 2 /* LOW_SHELF */: {
						coefficients = IIRCoefficients (/* b0 */ gain * tan_half_wc + sqrt_gain,
														/* b1 */ gain * tan_half_wc - sqrt_gain,
														/* b2 */ 0.0,
														/* a0 */ tan_half_wc + sqrt_gain,
														/* a1 */ tan_half_wc - sqrt_gain,
														/* a2 */ 0.0);
						break;
					}
					case 3 /* HIGH_SHELF */: {
						coefficients = IIRCoefficients (/* b0 */ sqrt_gain * tan_half_wc + gain,
														/* b1 */ sqrt_gain * tan_half_wc - gain,
														/* b2 */ 0.0,
														/* a0 */ sqrt_gain * tan_half_wc + 1.0,
														/* a1 */ sqrt_gain * tan_half_wc - 1.0,
														/* a2 */ 0.0);
						break;
					}
					case 4 /* BAND_PASS */: {
						coefficients = IIRCoefficients (/* b0 */ tan_half_bw,
														/* b1 */ 0.0,
														/* b2 */ -tan_half_bw,
														/* a0 */ 1.0 + tan_half_bw,
														/* a1 */ two_cos_wc,
														/* a2 */ 1.0 - tan_half_bw);
						break;
					}
					case 5 /* BAND_STOP */: {
						coefficients = IIRCoefficients (/* b0 */ 1.0,
														/* b1 */ two_cos_wc,
														/* b2 */ 1.0,
														/* a0 */ 1.0 + tan_half_bw,
														/* a1 */ two_cos_wc,
														/* a2 */ 1.0 - tan_half_bw);
						break;
					}
					case 6 /* PEAKING_NOTCH */: {
						coefficients = IIRCoefficients (/* b0 */ sqrt_gain + gain * tan_half_bw,
														/* b1 */ sqrt_gain * two_cos_wc,
														/* b2 */ sqrt_gain - gain * tan_half_bw,
														/* a0 */ sqrt_gain + tan_half_bw,
														/* a1 */ sqrt_gain * two_cos_wc,
														/* a2 */ sqrt_gain - tan_half_bw);
						break;
					}
				}

				setCoefficients (coefficients);
			}
		};

		juce::OwnedArray<Filter> filters;

		void updateFilters(int64_t frame_number, double sample_rate);
	};

}

#endif
