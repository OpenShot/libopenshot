/**
 * @file
 * @brief Source file for Wave effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Wave.h"
#include "Exceptions.h"
#include <algorithm>
#include <cmath>

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Wave::Wave() : wavelength(0.06), amplitude(0.3), multiplier(0.2), shift_x(0.0), speed_y(0.2), direction(0.0), warp_angle(0.0), displacement_angle(0.0) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Wave::Wave(Keyframe wavelength, Keyframe amplitude, Keyframe multiplier, Keyframe shift_x, Keyframe speed_y)
		: wavelength(wavelength), amplitude(amplitude), multiplier(multiplier), shift_x(shift_x), speed_y(speed_y), direction(0.0), warp_angle(0.0), displacement_angle(0.0)
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
std::shared_ptr<openshot::Frame> Wave::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{

    // Directional Wave modes (0 preserves legacy rendering).
    const int mode = static_cast<int>(
        std::lround(direction.GetValue(frame_number)));

    if (mode >= 1 && mode <= 3) {
        auto image = frame->GetImage();

        if (!image || image->isNull())
            return frame;

        const int width = image->width();
        const int height = image->height();

        const QImage original = image->copy();

        const double pi = 3.14159265358979323846;
        const double warp =
            warp_angle.GetValue(frame_number) * pi / 180.0;
        const double displacement =
            displacement_angle.GetValue(frame_number) * pi / 180.0;

        const double wc = std::cos(warp);
        const double ws = std::sin(warp);
        const double dc = std::cos(displacement);
        const double ds = std::sin(displacement);

        const double frequency = wavelength.GetValue(frame_number);
        const double strength =
            amplitude.GetValue(frame_number) *
            multiplier.GetValue(frame_number) * 100.0;

        const double phase =
            frame_number * speed_y.GetValue(frame_number);

        const double offset = shift_x.GetValue(frame_number);

        const double cx = (width - 1) / 2.0;
        const double cy = (height - 1) / 2.0;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const double coordinate =
                    (x - cx) * ws + (y - cy) * wc;

                const double wave =
                    (std::sin(coordinate * frequency + phase)
                     + offset) * strength;

                double dx = 0.0;
                double dy = 0.0;

                if (mode == 1) {
                    dx = wave;
                } else if (mode == 2) {
                    dy = wave;
                } else {
                    dx = wave * dc;
                    dy = wave * ds;
                }

                const double sample_x = std::max(
                    0.0, std::min(static_cast<double>(width - 1), x + dx));
                const double sample_y = std::max(
                    0.0, std::min(static_cast<double>(height - 1), y + dy));

                const int sx = static_cast<int>(std::lround(sample_x));
                const int sy = static_cast<int>(std::lround(sample_y));

                image->setPixel(x, y, original.pixel(sx, sy));
            }
        }

        return frame;
    }

	// Get the frame's image
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Copy original pixels for reference, and get a writable pointer for editing
	QImage original = frame_image->copy();
	const unsigned char *original_pixels = original.constBits();
	unsigned char *pixels = frame_image->bits();
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

		int source_px = lround(pixel + waveVal);
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
	root["direction"] = direction.JsonValue();
	root["warp_angle"] = warp_angle.JsonValue();
	root["displacement_angle"] = displacement_angle.JsonValue();

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
	if (!root["direction"].isNull())
		direction.SetJsonValue(root["direction"]);
	if (!root["warp_angle"].isNull())
		warp_angle.SetJsonValue(root["warp_angle"]);
	if (!root["displacement_angle"].isNull())
		displacement_angle.SetJsonValue(root["displacement_angle"]);
}

// Get all properties for a specific frame
std::string Wave::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Keyframes
	root["wavelength"] = add_property_json("Wave length", wavelength.GetValue(requested_frame), "float", "", &wavelength, 0.0, 3.0, false, requested_frame);
	root["amplitude"] = add_property_json("Amplitude", amplitude.GetValue(requested_frame), "float", "", &amplitude, 0.0, 5.0, false, requested_frame);
	root["multiplier"] = add_property_json("Multiplier", multiplier.GetValue(requested_frame), "float", "", &multiplier, 0.0, 10.0, false, requested_frame);
	root["shift_x"] = add_property_json("X Shift", shift_x.GetValue(requested_frame), "float", "", &shift_x, 0.0, 1000.0, false, requested_frame);
	root["speed_y"] = add_property_json("Vertical speed", speed_y.GetValue(requested_frame), "float", "", &speed_y, 0.0, 300.0, false, requested_frame);
	root["direction"] = add_property_json("Direction (0=Legacy, 1=Horizontal, 2=Vertical, 3=Custom Angle)", direction.GetValue(requested_frame), "int", "", &direction, 0, 3, false, requested_frame);
	root["warp_angle"] = add_property_json("Warp Angle", warp_angle.GetValue(requested_frame), "float", "", &warp_angle, -180.0, 180.0, false, requested_frame);
	root["displacement_angle"] = add_property_json("Displacement Angle", displacement_angle.GetValue(requested_frame), "float", "", &displacement_angle, -180.0, 180.0, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}
