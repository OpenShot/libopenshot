/**
 * @file
 * @brief Source file for Mask class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#include "../../include/effects/Mask.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Mask::Mask() : reader(NULL), replace_image(false) {
	// Init effect properties
	init_effect_details();
}

// Default constructor
Mask::Mask(ReaderBase *mask_reader, Keyframe mask_brightness, Keyframe mask_contrast) :
		reader(mask_reader), brightness(mask_brightness), contrast(mask_contrast), replace_image(false)
{
	// Init effect properties
	init_effect_details();
}

// Init effect settings
void Mask::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Mask";
	info.name = "Alpha Mask / Wipe Transition";
	info.description = "Uses a grayscale mask image to gradually wipe / transition between 2 images.";
	info.has_audio = false;
	info.has_video = true;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Mask::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number) {
	// Get the mask image (from the mask reader)
	std::shared_ptr<QImage> frame_image = frame->GetImage();

	// Check if mask reader is open
	#pragma omp critical (open_mask_reader)
	{
		if (reader && !reader->IsOpen())
			reader->Open();
	}

	// No reader (bail on applying the mask)
	if (!reader)
		return frame;

	// Get mask image (if missing or different size than frame image)
	#pragma omp critical (open_mask_reader)
	{
		if (!original_mask || !reader->info.has_single_image ||
			(original_mask && original_mask->size() != frame_image->size())) {

			// Only get mask if needed
			std::shared_ptr<QImage> mask_without_sizing = std::shared_ptr<QImage>(
					new QImage(*reader->GetFrame(frame_number)->GetImage()));

			// Resize mask image to match frame size
			original_mask = std::shared_ptr<QImage>(new QImage(
					mask_without_sizing->scaled(frame_image->width(), frame_image->height(), Qt::IgnoreAspectRatio,
												Qt::SmoothTransformation)));
		}
	}

	// Get pixel arrays
	unsigned char *pixels = (unsigned char *) frame_image->bits();
	unsigned char *mask_pixels = (unsigned char *) original_mask->bits();

	int R = 0;
	int G = 0;
	int B = 0;
	int A = 0;
	int gray_value = 0;
	float factor = 0.0;
	double contrast_value = (contrast.GetValue(frame_number));
	double brightness_value = (brightness.GetValue(frame_number));

	// Loop through mask pixels, and apply average gray value to frame alpha channel
	for (int pixel = 0, byte_index=0; pixel < original_mask->width() * original_mask->height(); pixel++, byte_index+=4)
	{
		// Get the RGB values from the pixel
		R = mask_pixels[byte_index];
		G = mask_pixels[byte_index + 1];
		B = mask_pixels[byte_index + 2];

		// Get the average luminosity
		gray_value = qGray(R, G, B);

		// Adjust the contrast
		factor = (259 * (contrast_value + 255)) / (255 * (259 - contrast_value));
		gray_value = constrain((factor * (gray_value - 128)) + 128);

		// Adjust the brightness
		gray_value += (255 * brightness_value);

		// Constrain the value from 0 to 255
		gray_value = constrain(gray_value);

		// Set the alpha channel to the gray value
		if (replace_image) {
			// Replace frame pixels with gray value
			pixels[byte_index + 0] = gray_value;
			pixels[byte_index + 1] = gray_value;
			pixels[byte_index + 2] = gray_value;
		} else {
			// Set alpha channel
			A = pixels[byte_index + 3];
			pixels[byte_index + 3] = constrain(A - gray_value);
		}

	}

	// return the modified frame
	return frame;
}

// Generate JSON string of this object
string Mask::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value Mask::JsonValue() {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["brightness"] = brightness.JsonValue();
	root["contrast"] = contrast.JsonValue();
	if (reader)
		root["reader"] = reader->JsonValue();
	else
		root["reader"] = Json::objectValue;
	root["replace_image"] = replace_image;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Mask::SetJson(string value) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void Mask::SetJsonValue(Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["replace_image"].isNull())
		replace_image = root["replace_image"].asBool();
	if (!root["brightness"].isNull())
		brightness.SetJsonValue(root["brightness"]);
	if (!root["contrast"].isNull())
		contrast.SetJsonValue(root["contrast"]);
	if (!root["reader"].isNull()) // does Json contain a reader?
	{

		if (!root["reader"]["type"].isNull()) // does the reader Json contain a 'type'?
		{
			// Close previous reader (if any)
			if (reader)
			{
				// Close and delete existing reader (if any)
				reader->Close();
				delete reader;
				reader = NULL;
			}

			// Create new reader (and load properties)
			string type = root["reader"]["type"].asString();

			if (type == "FFmpegReader") {

				// Create new reader
				reader = new FFmpegReader(root["reader"]["path"].asString());
				reader->SetJsonValue(root["reader"]);

#ifdef USE_IMAGEMAGICK
			} else if (type == "ImageReader") {

				// Create new reader
				reader = new ImageReader(root["reader"]["path"].asString());
				reader->SetJsonValue(root["reader"]);
#endif

			} else if (type == "QtImageReader") {

				// Create new reader
				reader = new QtImageReader(root["reader"]["path"].asString());
				reader->SetJsonValue(root["reader"]);

			} else if (type == "ChunkReader") {

				// Create new reader
				reader = new ChunkReader(root["reader"]["path"].asString(), (ChunkVersion) root["reader"]["chunk_version"].asInt());
				reader->SetJsonValue(root["reader"]);

			}

		}
	}

}

// Get all properties for a specific frame
string Mask::PropertiesJSON(int64_t requested_frame) {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 30 * 60 * 60 * 48, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 30 * 60 * 60 * 48, true, requested_frame);
	root["replace_image"] = add_property_json("Replace Image", replace_image, "int", "", NULL, 0, 1, false, requested_frame);

	// Add replace_image choices (dropdown style)
	root["replace_image"]["choices"].append(add_property_choice_json("Yes", true, replace_image));
	root["replace_image"]["choices"].append(add_property_choice_json("No", false, replace_image));

	// Keyframes
	root["brightness"] = add_property_json("Brightness", brightness.GetValue(requested_frame), "float", "", &brightness, -1.0, 1.0, false, requested_frame);
	root["contrast"] = add_property_json("Contrast", contrast.GetValue(requested_frame), "float", "", &contrast, 0, 20, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}

