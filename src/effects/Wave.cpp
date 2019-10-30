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

#include "../../include/effects/Wave.h"

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

	// Get pixels for frame image
	unsigned char *pixels = (unsigned char *) frame_image->bits();

	// Make temp copy of pixels before we start changing them
	unsigned char *temp_image = new unsigned char[frame_image->width() * frame_image->height() * 4]();
	memcpy(temp_image, pixels, sizeof(char) * frame_image->width() * frame_image->height() * 4);

	// Get current keyframe values
	double time = frame_number;//abs(((frame_number + 255) % 510) - 255);
	double wavelength_value = wavelength.GetValue(frame_number);
	double amplitude_value = amplitude.GetValue(frame_number);
	double multiplier_value = multiplier.GetValue(frame_number);
	double shift_x_value = shift_x.GetValue(frame_number);
	double speed_y_value = speed_y.GetValue(frame_number);

	// Loop through pixels
	for (int pixel = 0, byte_index=0; pixel < frame_image->width() * frame_image->height(); pixel++, byte_index+=4)
	{
		// Calculate X and Y pixel coordinates
		int Y = pixel / frame_image->width();

		// Calculate wave pixel offsets
		float noiseVal = (100 + Y * 0.001) * multiplier_value; // Time and time multiplier (to make the wave move)
		float noiseAmp = noiseVal * amplitude_value; // Apply amplitude / height of the wave
		float waveformVal = sin((Y * wavelength_value) + (time * speed_y_value)); // Waveform algorithm on y-axis
		float waveVal = (waveformVal + shift_x_value) * noiseAmp; // Shifts pixels on the x-axis

		int source_X = round(pixel + waveVal) * 4;
		if (source_X < 0)
			source_X = 0;
		if (source_X > frame_image->width() * frame_image->height() * 4 * sizeof(char))
			source_X = (frame_image->width() * frame_image->height() * 4 * sizeof(char)) - (sizeof(char) * 4);

		// Calculate source array location, and target array location, and copy the 4 color values
		memcpy(&pixels[byte_index], &temp_image[source_X], sizeof(char) * 4);
	}

	// Delete arrays
	delete[] temp_image;

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string Wave::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Wave::JsonValue() {

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
void Wave::SetJson(string value) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;
	Json::CharReader* reader(rbuilder.newCharReader());

	string errors;
	bool success = reader->parse( value.c_str(),
                 value.c_str() + value.size(), &root, &errors );
	delete reader;

	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::JsonValue into this object
void Wave::SetJsonValue(Json::Value root) {

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
string Wave::PropertiesJSON(int64_t requested_frame) {

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
