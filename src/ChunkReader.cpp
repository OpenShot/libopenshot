/**
 * @file
 * @brief Source file for ChunkReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "ChunkReader.h"
#include "QtJsonAdapter.h"
#include "Exceptions.h"
#include "FFmpegReader.h"

#include <QDir>
#include <QJsonObject>

using namespace openshot;

namespace {

// Preserve the JsonCpp scalar conversion behavior used by ChunkReader.
// JSON null and missing members convert to zero; strings are not numeric.
double jsonCompatNumber(const QJsonValue& value)
{
    if (value.isNull() || value.isUndefined())
        return 0.0;
    if (value.isBool())
        return value.toBool() ? 1.0 : 0.0;
    if (!value.isDouble())
        throw std::invalid_argument("Expected a JSON number");
    return value.toDouble();
}

int jsonCompatInt(const QJsonValue& value)
{
    const double number = jsonCompatNumber(value);
    if (!std::isfinite(number) ||
        number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max()))
        throw std::out_of_range("JSON integer is out of range");
    return static_cast<int>(number);
}

unsigned int jsonCompatUInt(const QJsonValue& value)
{
    const double number = jsonCompatNumber(value);
    if (!std::isfinite(number) || number < 0.0 ||
        number > static_cast<double>(std::numeric_limits<unsigned int>::max()))
        throw std::out_of_range("JSON unsigned integer is out of range");
    return static_cast<unsigned int>(number);
}

bool jsonCompatBool(const QJsonValue& value)
{
    return jsonCompatNumber(value) != 0.0;
}

} // namespace

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

    // Parse chunk metadata using Qt JSON.
    QJsonObject root;
    try
    {
        root = openshot::detail::parseQtJson(
            json_string.str()).object();
    }
    catch (const InvalidJSON&)
    {
        throw InvalidJSON(
            "Chunk folder could not be opened.", path);
    }

    // Populate metadata from the JSON object.
    try
    {
        info.has_video = jsonCompatBool(root.value("has_video"));
        info.has_audio = jsonCompatBool(root.value("has_audio"));
        info.duration = jsonCompatNumber(root.value("duration"));
        info.file_size = std::stoll(
            root.value("file_size").toString().toStdString());
        info.height = jsonCompatInt(root.value("height"));
        info.width = jsonCompatInt(root.value("width"));
        info.pixel_format = jsonCompatInt(root.value("pixel_format"));

        const QJsonObject fps =
            root.value("fps").toObject();
        info.fps.num = jsonCompatInt(fps.value("num"));
        info.fps.den = jsonCompatInt(fps.value("den"));

        info.video_bit_rate =
            jsonCompatUInt(root.value("video_bit_rate"));

        const QJsonObject pixel_ratio =
            root.value("pixel_ratio").toObject();
        info.pixel_ratio.num = jsonCompatInt(pixel_ratio.value("num"));
        info.pixel_ratio.den = jsonCompatInt(pixel_ratio.value("den"));

        const QJsonObject display_ratio =
            root.value("display_ratio").toObject();
        info.display_ratio.num = jsonCompatInt(display_ratio.value("num"));
        info.display_ratio.den = jsonCompatInt(display_ratio.value("den"));

        info.vcodec = root.value("vcodec").toString().toStdString();
        info.video_length = std::stoll(
            root.value("video_length").toString().toStdString());
        info.video_stream_index =
            jsonCompatInt(root.value("video_stream_index"));

        const QJsonObject video_timebase =
            root.value("video_timebase").toObject();
        info.video_timebase.num =
            jsonCompatInt(video_timebase.value("num"));
        info.video_timebase.den =
            jsonCompatInt(video_timebase.value("den"));

        info.interlaced_frame =
            jsonCompatBool(root.value("interlaced_frame"));
        info.top_field_first =
            jsonCompatBool(root.value("top_field_first"));

        info.acodec = root.value("acodec").toString().toStdString();
        info.audio_bit_rate =
            jsonCompatUInt(root.value("audio_bit_rate"));
        info.sample_rate =
            jsonCompatUInt(root.value("sample_rate"));
        info.channels = jsonCompatInt(root.value("channels"));
        info.audio_stream_index =
            jsonCompatInt(root.value("audio_stream_index"));

        const QJsonObject audio_timebase =
            root.value("audio_timebase").toObject();
        info.audio_timebase.num =
            jsonCompatInt(audio_timebase.value("num"));
        info.audio_timebase.den =
            jsonCompatInt(audio_timebase.value("den"));
    }
    catch (const std::exception&)
    {
        throw InvalidJSON(
            "JSON could not be parsed (or is invalid).", path);
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
