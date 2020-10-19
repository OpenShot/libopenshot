/**
 * @file
 * @brief Source file for Wave effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
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

#include "Wave.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Wave::Wave() : wavelength(0.06), amplitude(0.3), multiplier(0.2), shift_x(0.0), speed_y(0.2) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Wave::Wave(Keyframe wavelength, Keyframe amplitude, Keyframe multiplier, Keyframe shift_x, Keyframe speed_y)
		: wavelength(wavelength), amplitude(amplitude), multiplier(multiplier), shift_x(shift_x), speed_y(speed_y)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Wave::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Wave";
	info.name = "Wave";
	info.description = "Distort the frame's image into a wave pattern.";
	info.has_audio = false;
	info.has_video = true;

}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Wave::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{
	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Get original pixels for frame image, and also make a copy for editing
	const unsigned char *original_pixels = (unsigned char *) frame_image->constBits();
	unsigned char *pixels = (unsigned char *) frame_image->bits();
	int pixel_count = frame_image->width() * frame_image->height();

	// Get current keyframe values
	double time = frame_number;
	double wavelength_value = wavelength.GetValue(frame_number);
	double amplitude_value = amplitude.GetValue(frame_number);
	double multiplier_value = multiplier.GetValue(frame_number);
	double shift_x_value = shift_x.GetValue(frame_number);
	double speed_y_value = speed_y.GetValue(frame_number);

	// Loop through pixels
	#pragma omp parallel for
	for (int pixel = 0; pixel < pixel_count; ++pixel)
	{
		// Calculate pixel Y value
		int Y = pixel / frame_image->width();

		// Calculate wave pixel offsets
		float noiseVal = (100 + Y * 0.001) * multiplier_value;  // Time and time multiplier (to make the wave move)
		float noiseAmp = noiseVal * amplitude_value;  // Apply amplitude / height of the wave
		float waveformVal = sin((Y * wavelength_value) + (time * speed_y_value));  // Waveform algorithm on y-axis
		float waveVal = (waveformVal + shift_x_value) * noiseAmp;  // Shifts pixels on the x-axis

		long unsigned int source_px = round(pixel + waveVal);
		if (source_px < 0)
			source_px = 0;
		if (source_px >= pixel_count)
			source_px = pixel_count - 1;

		// Calculate source array location, and target array location, and copy the 4 color values
		memcpy(&pixels[pixel * 4], &original_pixels[source_px * 4], sizeof(char) * 4);
	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Wave::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Wave::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["wavelength"] = wavelength.JsonValue();
	root["amplitude"] = amplitude.JsonValue();
	root["multiplier"] = multiplier.JsonValue();
	root["shift_x"] = shift_x.JsonValue();
	root["speed_y"] = speed_y.JsonValue();

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Wave::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void Wave::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["wavelength"].isNull())
		wavelength.SetJsonValue(root["wavelength"]);
	if (!root["amplitude"].isNull())
		amplitude.SetJsonValue(root["amplitude"]);
	if (!root["multiplier"].isNull())
		multiplier.SetJsonValue(root["multiplier"]);
	if (!root["shift_x"].isNull())
		shift_x.SetJsonValue(root["shift_x"]);
	if (!root["speed_y"].isNull())
		speed_y.SetJsonValue(root["speed_y"]);
}

// Get all properties for a specific frame
std::string Wave::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["wavelength"] = add_property_json("Wave length", wavelength.GetValue(requested_frame), "float", "", &wavelength, 0.0, 3.0, false, requested_frame);
	root["amplitude"] = add_property_json("Amplitude", amplitude.GetValue(requested_frame), "float", "", &amplitude, 0.0, 5.0, false, requested_frame);
	root["multiplier"] = add_property_json("Multiplier", multiplier.GetValue(requested_frame), "float", "", &multiplier, 0.0, 10.0, false, requested_frame);
	root["shift_x"] = add_property_json("X Shift", shift_x.GetValue(requested_frame), "float", "", &shift_x, 0.0, 1000.0, false, requested_frame);
	root["speed_y"] = add_property_json("Vertical speed", speed_y.GetValue(requested_frame), "float", "", &speed_y, 0.0, 300.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
