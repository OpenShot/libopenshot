/**
 * @file
 * @brief Header file for FilmGrain effect
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_FILM_GRAIN_EFFECT_H
#define OPENSHOT_FILM_GRAIN_EFFECT_H

#include "../EffectBase.h"
#include "../Frame.h"
#include "../Json.h"
#include "../KeyFrame.h"

#include <memory>
#include <string>

namespace openshot
{
	/**
	 * @brief Creative film-inspired grain texture effect.
	 *
	 * FilmGrain exposes a compact set of visible controls for structure,
	 * tonal response, color behavior, temporal behavior, and deterministic
	 * variation. Presets should initialize these same properties rather than
	 * selecting hidden internal modes.
	 */
	class FilmGrain : public EffectBase
	{
	private:
		void init_effect_details();

	public:
		Keyframe amount;           ///< Overall grain intensity.
		Keyframe size;             ///< Fine to coarse grain scale.
		Keyframe softness;         ///< Crisp vs soft organic grain.
		Keyframe clump;            ///< Even vs clustered irregular structure.
		Keyframe shadows;          ///< Grain strength in dark regions.
		Keyframe midtones;         ///< Grain strength in middle tonal regions.
		Keyframe highlights;       ///< Grain strength in bright regions.
		Keyframe color_amount;     ///< Chroma contribution amount.
		Keyframe color_variation;  ///< RGB channel independence.
		Keyframe evolution;        ///< How much the grain renews over time.
		Keyframe coherence;        ///< Smoothness/stability between frames.
		int seed;                  ///< Deterministic grain identity.

		FilmGrain();

		std::shared_ptr<openshot::Frame> GetFrame(int64_t frame_number) override {
			return GetFrame(std::make_shared<openshot::Frame>(), frame_number);
		}
		std::shared_ptr<openshot::Frame> GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number) override;

		std::string Json() const override;
		void SetJson(const std::string value) override;
		Json::Value JsonValue() const override;
		void SetJsonValue(const Json::Value root) override;
		std::string PropertiesJSON(int64_t requested_frame) const override;
	};
}

#endif
