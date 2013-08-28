#include "../include/ChunkReader.h"

using namespace openshot;

ChunkReader::ChunkReader(string path) throw(InvalidFile, InvalidJSON)
		: path(path), is_open(false)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Check if folder exists?
	if (!does_folder_exist(path))
		// Raise exception
		throw InvalidFile("Chunk folder could not be opened.", path);

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Check if folder path existing
bool ChunkReader::does_folder_exist(string path)
{
	QDir dir(path.c_str());
	return dir.exists();
}

// Load JSON meta data about this chunk folder
void ChunkReader::load_json()
{
	// Load path of chunk folder
	string json_path = QDir::cleanPath(QString(path.c_str()) + QDir::separator() + "info.json").toStdString();
	stringstream json_string;

	// Read the JSON file
	ifstream myfile (json_path.c_str());
	string line = "";
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
	Json::Reader reader;
	bool success = reader.parse( json_string.str(), root );
	if (!success)
		// Raise exception
		throw InvalidJSON("Chunk folder could not be opened.", path);


	// Set info from the JSON objects
	try
	{
		info.has_video = root["has_video"].asBool();
		info.has_audio = root["has_audio"].asBool();
		info.duration = root["duration"].asDouble();
		info.file_size = atoll(root["file_size"].asString().c_str());
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
		info.video_length = atoll(root["video_length"].asString().c_str());
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
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON could not be parsed (or is invalid).", path);
	}
}

// Open chunk folder or file
void ChunkReader::Open() throw(InvalidFile)
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

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> ChunkReader::GetFrame(int requested_frame) throw(ReaderClosed)
{
	if (1==1)
	{



	}
	else
		// no frame loaded
		throw InvalidFile("No frame could be created from this type of file.", path);
}


