/**
 * @file
 * @brief Source file for De-interlace class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Deinterlace.h"
#include "Exceptions.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Deinterlace::Deinterlace() : isOdd(true)
{
	// Init effect properties
	init_effect_details();
}

// Default constructor
Deinterlace::Deinterlace(bool UseOddLines) : isOdd(UseOddLines)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Deinterlace::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Deinterlace";
	info.name = "Deinterlace";
	info.description = "Remove interlacing from a video (i.e. even or odd horizontal lines)";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<openshot::Frame> Deinterlace::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	// Get original size of frame's image
	int original_width = frame->GetImage()->width();
	int original_height = frame->GetImage()->height();

	// Get the frame's image
	std::shared_ptr<QImage> image = frame->GetImage();
	const unsigned char* pixels = image->bits();

	// Create a smaller, new image
	QImage deinterlaced_image(image->width(), image->height() / 2, QImage::Format_RGBA8888_Premultiplied);
	const unsigned char* deinterlaced_pixels = deinterlaced_image.bits();

	// Loop through the scanlines of the image (even or odd)
	int start = 0;
	if (isOdd)
		start = 1;
	for (int row = start; row < image->height(); row += 2) {
		memcpy((unsigned char*)deinterlaced_pixels, pixels + (row * image->bytesPerLine()), image->bytesPerLine());
		deinterlaced_pixels += image->bytesPerLine();
	}

	// Resize deinterlaced image back to original size, and update frame's image
	image = std::make_shared<QImage>(deinterlaced_image.scaled(
		original_width, original_height,
		Qt::IgnoreAspectRatio, Qt::FastTransformation));

	// Update image on frame
	frame->AddImage(image);

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
std::string Deinterlace::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Deinterlace::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["isOdd"] = isOdd;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Deinterlace::SetJson(const std::string value) {

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
void Deinterlace::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["isOdd"].isNull())
		isOdd = root["isOdd"].asBool();
}

// Get all properties for a specific frame
std::string Deinterlace::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root = BasePropertiesJSON(requested_frame);

	// Add Is Odd Frame choices (dropdown style)
	root["isOdd"] = add_property_json("Is Odd Frame", isOdd, "bool", "", NULL, 0, 1, true, requested_frame);
	root["isOdd"]["choices"].append(add_property_choice_json("Yes", true, isOdd));
	root["isOdd"]["choices"].append(add_property_choice_json("No", false, isOdd));

	// Return formatted string
	return root.toStyledString();
}
