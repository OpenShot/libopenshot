/**
 * @file
 * @brief Source file for Profile class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <iomanip>
#include "Profiles.h"
#include "Exceptions.h"

using namespace openshot;

// default constructor
Profile::Profile() {
	// Initialize info values
	info.description = "";
	info.height = 0;
	info.width = 0;
	info.pixel_format = 0;
	info.fps.num = 0;
	info.fps.den = 0;
	info.pixel_ratio.num = 0;
	info.pixel_ratio.den = 0;
	info.display_ratio.num = 0;
	info.display_ratio.den = 0;
	info.interlaced_frame = false;
	info.spherical = false; // Default to non-spherical (regular) video
}

// @brief Constructor for Profile.
// @param path 	The folder path / location of a profile file
Profile::Profile(std::string path) {

	bool read_file = false;

	// Initialize all values to defaults (same as default constructor)
	info.description = "";
	info.height = 0;
	info.width = 0;
	info.pixel_format = 0;
	info.fps.num = 0;
	info.fps.den = 0;
	info.pixel_ratio.num = 0;
	info.pixel_ratio.den = 0;
	info.display_ratio.num = 0;
	info.display_ratio.den = 0;
	info.interlaced_frame = false;
	info.spherical = false;  // Default to non-spherical (regular) video

	try
	{
		QFile inputFile(path.c_str());
		if (inputFile.open(QIODevice::ReadOnly))
		{
			QTextStream in(&inputFile);
			while (!in.atEnd())
			{
				QString line = in.readLine();

				if (line.length() <= 0)
					continue;

				// Split current line
				QStringList parts = line.split( "=" );
				std::string setting = parts[0].toStdString();
				std::string value = parts[1].toStdString();
				int value_int = 0;

				// update struct (based on line number)
				if (setting == "description") {
					info.description = value;
				}
				else if (setting == "frame_rate_num") {
					value_int = std::stoi(value);
					info.fps.num = value_int;
				}
				else if (setting == "frame_rate_den") {
					value_int = std::stoi(value);
					info.fps.den = value_int;
				}
				else if (setting == "width") {
					value_int = std::stoi(value);
					info.width = value_int;
				}
				else if (setting == "height") {
					value_int = std::stoi(value);
					info.height = value_int;
				}
				else if (setting == "progressive") {
					value_int = std::stoi(value);
					info.interlaced_frame = !(bool)value_int;
				}
				else if (setting == "sample_aspect_num") {
					value_int = std::stoi(value);
					info.pixel_ratio.num = value_int;
				}
				else if (setting == "sample_aspect_den") {
					value_int = std::stoi(value);
					info.pixel_ratio.den = value_int;
				}
				else if (setting == "display_aspect_num") {
					value_int = std::stoi(value);
					info.display_ratio.num = value_int;
				}
				else if (setting == "display_aspect_den") {
					value_int = std::stoi(value);
					info.display_ratio.den = value_int;
				}
				else if (setting == "colorspace") {
					value_int = std::stoi(value);
					info.pixel_format = value_int;
				}
				else if (setting == "spherical") {
					value_int = std::stoi(value);
					info.spherical = (bool)value_int;
				}
			}
			read_file = true;
			inputFile.close();
		}

	}
	catch (const std::exception& e)
	{
		// Error parsing profile file
		throw InvalidFile("Profile could not be found or loaded (or is invalid).", path);
	}

	// Throw error if file was not read
	if (!read_file)
		// Error parsing profile file
		throw InvalidFile("Profile could not be found or loaded (or is invalid).", path);
}

// Return a formatted FPS
std::string Profile::formattedFPS(bool include_decimal) {
	// Format FPS to use 2 decimals (if needed)
	if (!include_decimal) {
		int fps_code = 0;

		if (info.fps.den == 1) {
			// Exact integer FPS (e.g. 24 → 0024)
			fps_code = info.fps.num;
		} else {
			// Fractional FPS, scale by 100 (e.g. 29.97 → 2997)
			fps_code = static_cast<int>((info.fps.num * 100.0) / info.fps.den + 0.5);
		}

		char buffer[5];
		std::snprintf(buffer, sizeof(buffer), "%04d", fps_code);
		return std::string(buffer);
	}

	// Human-readable version for display
	float fps = info.fps.ToFloat();

	if (std::fabs(fps - std::round(fps)) < 0.01) {
		return std::to_string(static_cast<int>(std::round(fps)));
	}

	char buffer[16];
	std::snprintf(buffer, sizeof(buffer), "%.2f", fps);
	return std::string(buffer);
}

// Return a unique key of this profile (01920x1080i2997_16-09)
std::string Profile::Key() {
	std::string raw_fps = formattedFPS(false);

	// Pad FPS string to 4 characters with leading zeros
	std::string fps_padded = std::string(4 - raw_fps.length(), '0') + raw_fps;

	char buffer[64];
	std::snprintf(buffer, sizeof(buffer), "%05dx%04d%s%s_%02d-%02d",
		info.width,
		info.height,
		info.interlaced_frame ? "i" : "p",
		fps_padded.c_str(),
		info.display_ratio.num,
		info.display_ratio.den
	);

	std::string result(buffer);
	if (info.spherical)
		result += "_360";
	return result;
}

// Return the name of this profile (1920x1080p29.97)
std::string Profile::ShortName() {
	std::string progressive_str = info.interlaced_frame ? "i" : "p";
	std::string fps_string = formattedFPS(true);
	std::string result = std::to_string(info.width) + "x" + std::to_string(info.height) + progressive_str + fps_string;

	if (info.spherical)
		result += " 360°";
	return result;
}

// Return a longer format name (1920x1080p @ 29.97 fps (16:9))
std::string Profile::LongName() {
	std::string progressive_str = info.interlaced_frame ? "i" : "p";
	std::string fps_string = formattedFPS(true);
	std::string result = std::to_string(info.width) + "x" + std::to_string(info.height) +
		progressive_str + " @ " + fps_string +
		" fps (" + std::to_string(info.display_ratio.num) + ":" +
		std::to_string(info.display_ratio.den) + ")";

	if (info.spherical)
		result += " 360°";
	return result;
}

// Return a longer format name (1920x1080p @ 29.97 fps (16:9) HD 1080i 29.97 fps)
std::string Profile::LongNameWithDesc() {
	std::string progressive_str = info.interlaced_frame ? "i" : "p";
	std::string fps_string = formattedFPS(true);

	std::string result = std::to_string(info.width) + "x" + std::to_string(info.height) +
		progressive_str + " @ " + fps_string +
		" fps (" + std::to_string(info.display_ratio.num) + ":" +
		std::to_string(info.display_ratio.den) + ")";

	if (info.spherical)
		result += " 360°";

	if (!info.description.empty())
		result += " " + info.description;

	return result;
}

// Save profile to file system
void Profile::Save(const std::string& file_path) const {
	std::ofstream file(file_path);
	if (!file.is_open()) {
		throw std::ios_base::failure("Failed to save profile.");
	}

	file << "description=" << info.description << "\n";
	file << "frame_rate_num=" << info.fps.num << "\n";
	file << "frame_rate_den=" << info.fps.den << "\n";
	file << "width=" << info.width << "\n";
	file << "height=" << info.height << "\n";
	file << "progressive=" << !info.interlaced_frame << "\n";  // Correct the boolean value for progressive/interlaced
	file << "sample_aspect_num=" << info.pixel_ratio.num << "\n";
	file << "sample_aspect_den=" << info.pixel_ratio.den << "\n";
	file << "display_aspect_num=" << info.display_ratio.num << "\n";
	file << "display_aspect_den=" << info.display_ratio.den << "\n";
	file << "pixel_format=" << info.pixel_format << "\n";
	file << "spherical=" << info.spherical;

	file.close();
}

// Generate JSON string of this object
std::string Profile::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Profile::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["description"] = info.description;
	root["height"] = info.height;
	root["width"] = info.width;
	root["pixel_format"] = info.pixel_format;
	root["fps"] = Json::Value(Json::objectValue);
	root["fps"]["num"] = info.fps.num;
	root["fps"]["den"] = info.fps.den;
	root["pixel_ratio"] = Json::Value(Json::objectValue);
	root["pixel_ratio"]["num"] = info.pixel_ratio.num;
	root["pixel_ratio"]["den"] = info.pixel_ratio.den;
	root["display_ratio"] = Json::Value(Json::objectValue);
	root["display_ratio"]["num"] = info.display_ratio.num;
	root["display_ratio"]["den"] = info.display_ratio.den;
	root["progressive"] = !info.interlaced_frame;
	root["spherical"] = info.spherical;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Profile::SetJson(const std::string value) {

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
void Profile::SetJsonValue(const Json::Value root) {

	if (!root["description"].isNull())
		info.description = root["description"].asString();
	if (!root["height"].isNull())
		info.height = root["height"].asInt();
	if (!root["width"].isNull())
		info.width = root["width"].asInt();
	if (!root["pixel_format"].isNull())
		info.pixel_format = root["pixel_format"].asInt();
	if (!root["fps"].isNull()) {
		info.fps.num = root["fps"]["num"].asInt();
		info.fps.den = root["fps"]["den"].asInt();
	}
	if (!root["pixel_ratio"].isNull()) {
		info.pixel_ratio.num = root["pixel_ratio"]["num"].asInt();
		info.pixel_ratio.den = root["pixel_ratio"]["den"].asInt();
		info.pixel_ratio.Reduce();
	}
	if (!root["display_ratio"].isNull()) {
		info.display_ratio.num = root["display_ratio"]["num"].asInt();
		info.display_ratio.den = root["display_ratio"]["den"].asInt();
		info.display_ratio.Reduce();
	}
	if (!root["progressive"].isNull())
		info.interlaced_frame = !root["progressive"].asBool();
	if (!root["spherical"].isNull())
		info.spherical = root["spherical"].asBool();

}
