/**
 * @file
 * @brief Source file for EffectInfo class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "EffectInfo.h"
#include "Effects.h"

using namespace openshot;

// Generate JSON string of this object
std::string EffectInfo::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Create a new effect instance
EffectBase* EffectInfo::CreateEffect(std::string effect_type) {
	// Init the matching effect object
	if (effect_type == "Bars")
		return new Bars();

	if (effect_type == "Blur")
		return new Blur();

	else if (effect_type == "Brightness")
		return new Brightness();

	else if (effect_type == "Caption")
		return new Caption();

	else if (effect_type == "ChromaKey")
		return new ChromaKey();

	else if (effect_type == "ColorMap")
		return new ColorMap();

	else if (effect_type == "ColorShift")
		return new ColorShift();

	else if (effect_type == "Crop")
		return new Crop();

	else if (effect_type == "Deinterlace")
		return new Deinterlace();

	else if (effect_type == "Hue")
		return new Hue();

	else if (effect_type == "LensFlare")
		return new LensFlare();

	else if (effect_type == "Mask")
		return new Mask();

	else if (effect_type == "Negate")
		return new Negate();

	else if (effect_type == "Pixelate")
		return new Pixelate();

	else if (effect_type == "Saturation")
		return new Saturation();

	else if (effect_type == "Sharpen")
		return new Sharpen();

	else if (effect_type == "Shift")
		return new Shift();

	else if (effect_type == "SphericalProjection")
		return new SphericalProjection();

	else if (effect_type == "Wave")
		return new Wave();

	else if(effect_type == "Noise")
		return new Noise();

	else if(effect_type == "Delay")
		return new Delay();

	else if(effect_type == "Echo")
		return new Echo();

	else if(effect_type == "Distortion")
		return new Distortion();

	else if(effect_type == "ParametricEQ")
		return new ParametricEQ();

	else if(effect_type == "Compressor")
		return new Compressor();

	else if(effect_type == "Expander")
		return new Expander();

	else if(effect_type == "Robotization")
		return new Robotization();

	else if(effect_type == "Whisperization")
		return new Whisperization();

	#ifdef USE_OPENCV
	else if (effect_type == "Outline")
		return new Outline();
	
	else if(effect_type == "Stabilizer")
		return new Stabilizer();

	else if(effect_type == "Tracker")
		return new Tracker();

	else if(effect_type == "ObjectDetection")
		return new ObjectDetection();
	#endif

	return NULL;
}

// Generate Json::Value for this object
Json::Value EffectInfo::JsonValue() {

	// Create root json object
	Json::Value root;

	// Append info JSON from each supported effect
	root.append(Bars().JsonInfo());
	root.append(Blur().JsonInfo());
	root.append(Brightness().JsonInfo());
	root.append(Caption().JsonInfo());
	root.append(ChromaKey().JsonInfo());
	root.append(ColorMap().JsonInfo());
	root.append(ColorShift().JsonInfo());
	root.append(Crop().JsonInfo());
	root.append(Deinterlace().JsonInfo());
	root.append(Hue().JsonInfo());
	root.append(LensFlare().JsonInfo());
	root.append(Mask().JsonInfo());
	root.append(Negate().JsonInfo());
	root.append(Pixelate().JsonInfo());
	root.append(Saturation().JsonInfo());
	root.append(Sharpen().JsonInfo());
	root.append(Shift().JsonInfo());
	root.append(SphericalProjection().JsonInfo());
	root.append(Wave().JsonInfo());
	/* Audio */
	root.append(Noise().JsonInfo());
	root.append(Delay().JsonInfo());
	root.append(Echo().JsonInfo());
	root.append(Distortion().JsonInfo());
	root.append(ParametricEQ().JsonInfo());
	root.append(Compressor().JsonInfo());
	root.append(Expander().JsonInfo());
	root.append(Robotization().JsonInfo());
	root.append(Whisperization().JsonInfo());

	#ifdef USE_OPENCV
	root.append(Outline().JsonInfo());
	root.append(Stabilizer().JsonInfo());	
	root.append(Tracker().JsonInfo());
	root.append(ObjectDetection().JsonInfo());
	#endif

	// return JsonValue
	return root;

}
