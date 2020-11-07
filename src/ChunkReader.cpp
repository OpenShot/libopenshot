/**
 * @file
 * @brief Source file for ChunkReader class
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

#include "ChunkReader.h"
#include "FFmpegReader.h"

#include <QDir>

using namespace openshot;

ChunkReader::ChunkReader(std::string path, ChunkVersion chunk_version)
		: path(path), chunk_size(24 * 3), is_open(false), version(chunk_version), local_reader(NULL)
{
	// Check if folder exists?
	if (!does_folder_exist(path))
		// Raise exception
		throw InvalidFile("Chunk folder could not be opened.", path);

	// Init previous location
	previous_location.number = 0;
	previous_location.frame = 0;

	// Open and Close the reader, to populate its attributes (such as height, width, etc...)
	Open();
	Close();
}

// Check if folder path existing
bool ChunkReader::does_folder_exist(std::string path)
{
	QDir dir(path.c_str());
	return dir.exists();
}

// Load JSON meta data about this chunk folder
void ChunkReader::load_json()
{
	// Load path of chunk folder
	std::string json_path = QDir::cleanPath(QString(path.c_str()) + QDir::separator() + "info.json").toStdString();
	std::stringstream json_string;

	// Read the JSON file
	std::ifstream myfile (json_path.c_str());
	std::string line = "";
	if (myfile.is_open())
	{
		while (myfile.good())
		{
			getline (myfile, line);
			json_string << line;
		}
		myfile.close();
	}

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::CharReaderBuilder rbuilder;

	std::string errors;
	bool success = Json::parseFromStream(rbuilder, json_string, &root, &errors);
	if (!success)
		// Raise exception
		throw InvalidJSON("Chunk folder could not be opened.", path);


	// Set info from the JSON objects
	try
	{
		info.has_video = root["has_video"].asBool();
		info.has_audio = root["has_audio"].asBool();
		info.duration = root["duration"].asDouble();
		info.file_size = std::stoll(root["file_size"].asString());
		info.height = root["height"].asInt();
		info.width = root["width"].asInt();
		info.pixel_format = root["pixel_format"].asInt();
		info.fps.num = root["fps"]["num"].asInt();
		info.fps.den = root["fps"]["den"].asInt();
		info.video_bit_rate = root["video_bit_rate"].asUInt();
		info.pixel_ratio.num = root["pixel_ratio"]["num"].asInt();
		info.pixel_ratio.den = root["pixel_ratio"]["den"].asInt();
		info.display_ratio.num = root["display_ratio"]["num"].asInt();
		info.display_ratio.den = root["display_ratio"]["den"].asInt();
		info.vcodec = root["vcodec"].asString();
		info.video_length = std::stoll(root["video_length"].asString());
		info.video_stream_index = root["video_stream_index"].asInt();
		info.video_timebase.num = root["video_timebase"]["num"].asInt();
		info.video_timebase.den = root["video_timebase"]["den"].asInt();
		info.interlaced_frame = root["interlaced_frame"].asBool();
		info.top_field_first = root["top_field_first"].asBool();
		info.acodec = root["acodec"].asString();
		info.audio_bit_rate = root["audio_bit_rate"].asUInt();
		info.sample_rate = root["sample_rate"].asUInt();
		info.channels = root["channels"].asInt();
		info.audio_stream_index = root["audio_stream_index"].asInt();
		info.audio_timebase.num = root["audio_timebase"]["num"].asInt();
		info.audio_timebase.den = root["audio_timebase"]["den"].asInt();

	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON could not be parsed (or is invalid).", path);
	}
}

// Find the location of a frame in a chunk
ChunkLocation ChunkReader::find_chunk_frame(int64_t requested_frame)
{
	// Determine which chunk contains this frame.
	int64_t chunk_number = (requested_frame / chunk_size) + 1;

	// Determine which frame in this chunk
	int64_t start_frame_of_chunk = (chunk_number - 1) * chunk_size;
	int64_t chunk_frame_number = (requested_frame - start_frame_of_chunk) + 1; // Add 1 to adjust for the 1st frame of every chunk is just there to "stoke" the audio samples from the previous chunk.

	// Prepare chunk location struct
	ChunkLocation location = {chunk_number, chunk_frame_number};

	return location;
}

// Open chunk folder or file
void ChunkReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		// parse JSON and load info.json file
		load_json();

		// Mark as "open"
		is_open = true;
	}
}

// Close image file
void ChunkReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;
	}
}

// get a formatted path of a specific chunk
std::string ChunkReader::get_chunk_path(int64_t chunk_number, std::string folder, std::string extension)
{
	// Create path of new chunk video
	std::stringstream chunk_count_string;
	chunk_count_string << chunk_number;
	QString padded_count = "%1"; //chunk_count_string.str().c_str();
	padded_count = padded_count.arg(chunk_count_string.str().c_str(), 6, '0');
	if (folder.length() != 0 && extension.length() != 0)
		// Return path with FOLDER and EXTENSION name
		return QDir::cleanPath(QString(path.c_str()) + QDir::separator() + folder.c_str() + QDir::separator() + padded_count + extension.c_str()).toStdString();

	else if (folder.length() == 0 && extension.length() != 0)
		// Return path with NO FOLDER and EXTENSION name
		return QDir::cleanPath(QString(path.c_str()) + QDir::separator() + padded_count + extension.c_str()).toStdString();

	else if (folder.length() != 0 && extension.length() == 0)
		// Return path with FOLDER and NO EXTENSION
		return QDir::cleanPath(QString(path.c_str()) + QDir::separator() + folder.c_str()).toStdString();
	else
		return "";
}

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> ChunkReader::GetFrame(int64_t requested_frame)
{
	// Determine what chunk contains this frame
	ChunkLocation location = find_chunk_frame(requested_frame);

	// New Chunk (Close the old reader, and open the new one)
	if (previous_location.number != location.number)
	{
		// Determine version of chunk
		std::string folder_name = "";
		switch (version)
		{
		case THUMBNAIL:
			folder_name = "thumb";
			break;
		case PREVIEW:
			folder_name = "preview";
			break;
		case FINAL:
			folder_name = "final";
			break;
		}

		// Load path of chunk video
		std::string chunk_video_path = get_chunk_path(location.number, folder_name, ".webm");

		// Close existing reader (if needed)
		if (local_reader)
		{
			// Close and delete old reader
			local_reader->Close();
			delete local_reader;
		}

		try
		{
			// Load new FFmpegReader
			local_reader = new FFmpegReader(chunk_video_path);
			local_reader->Open(); // open reader

		} catch (const InvalidFile& e)
		{
			// Invalid Chunk (possibly it is not found)
			throw ChunkNotFound(path, requested_frame, location.number, location.frame);
		}

		// Set the new location
		previous_location = location;
	}

	// Get the frame (from the current reader)
	last_frame = local_reader->GetFrame(location.frame);

	// Update the frame number property
	last_frame->number = requested_frame;

	// Return the frame
	return last_frame;
}

// Generate JSON string of this object
std::string ChunkReader::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value ChunkReader::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "ChunkReader";
	root["path"] = path;
	std::stringstream chunk_size_stream;
	chunk_size_stream << chunk_size;
	root["chunk_size"] = chunk_size_stream.str();
	root["chunk_version"] = version;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void ChunkReader::SetJson(const std::string value) {

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
void ChunkReader::SetJsonValue(const Json::Value root) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["path"].isNull())
		path = root["path"].asString();
	if (!root["chunk_size"].isNull())
		chunk_size = std::stoll(root["chunk_size"].asString());
	if (!root["chunk_version"].isNull())
		version = (ChunkVersion) root["chunk_version"].asInt();

	// Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
