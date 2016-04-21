/**
 * @file
 * @brief Source file for WriterBase class
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

#include "../include/WriterBase.h"

using namespace openshot;

// Constructor
WriterBase::WriterBase()
{
	// Initialized writer info
	info.has_video = false;
	info.has_audio = false;
	info.has_single_image = false;
	info.duration = 0.0;
	info.file_size = 0;
	info.height = 0;
	info.width = 0;
	info.pixel_format = -1;
	info.fps = Fraction();
	info.video_bit_rate = 0;
	info.pixel_ratio = Fraction();
	info.display_ratio = Fraction();
	info.vcodec = "";
	info.video_length = 0;
	info.video_stream_index = -1;
	info.video_timebase = Fraction();
	info.interlaced_frame = false;
	info.top_field_first = true;
	info.acodec = "";
	info.audio_bit_rate = 0;
	info.sample_rate = 0;
	info.channels = 0;
	info.channel_layout = LAYOUT_MONO;
	info.audio_stream_index = -1;
	info.audio_timebase = Fraction();
}

// This method copy's the info struct of a reader, and sets the writer with the same info
void WriterBase::CopyReaderInfo(ReaderBase* reader)
{
	info.has_video = reader->info.has_video;
	info.has_audio = reader->info.has_audio;
	info.has_single_image = reader->info.has_single_image;
	info.duration = reader->info.duration;
	info.file_size = reader->info.file_size;
	info.height = reader->info.height;
	info.width = reader->info.width;
	info.pixel_format = reader->info.pixel_format;
	info.fps.num = reader->info.fps.num;
	info.fps.den = reader->info.fps.den;
	info.video_bit_rate = reader->info.video_bit_rate;
	info.pixel_ratio.num = reader->info.pixel_ratio.num;
	info.pixel_ratio.den = reader->info.pixel_ratio.den;
	info.display_ratio.num = reader->info.display_ratio.num;
	info.display_ratio.den = reader->info.display_ratio.den;
	info.vcodec = reader->info.vcodec;
	info.video_length = reader->info.video_length;
	info.video_stream_index = reader->info.video_stream_index;
	info.video_timebase.num = reader->info.video_timebase.num;
	info.video_timebase.den = reader->info.video_timebase.den;
	info.interlaced_frame = reader->info.interlaced_frame;
	info.top_field_first = reader->info.top_field_first;
	info.acodec = reader->info.acodec;
	info.audio_bit_rate = reader->info.audio_bit_rate;
	info.sample_rate = reader->info.sample_rate;
	info.channels = reader->info.channels;
	info.channel_layout = reader->info.channel_layout;
	info.audio_stream_index = reader->info.audio_stream_index;
	info.audio_timebase.num = reader->info.audio_timebase.num;
	info.audio_timebase.den = reader->info.audio_timebase.den;
}

// Display file information
void WriterBase::DisplayInfo() {
	cout << fixed << setprecision(2) << boolalpha;
	cout << "----------------------------" << endl;
	cout << "----- File Information -----" << endl;
	cout << "----------------------------" << endl;
	cout << "--> Has Video: " << info.has_video << endl;
	cout << "--> Has Audio: " << info.has_audio << endl;
	cout << "--> Has Single Image: " << info.has_single_image << endl;
	cout << "--> Duration: " << info.duration << " Seconds" << endl;
	cout << "--> File Size: " << double(info.file_size) / 1024 / 1024 << " MB" << endl;
	cout << "----------------------------" << endl;
	cout << "----- Video Attributes -----" << endl;
	cout << "----------------------------" << endl;
	cout << "--> Width: " << info.width << endl;
	cout << "--> Height: " << info.height << endl;
	cout << "--> Pixel Format: " << info.pixel_format << endl;
	cout << "--> Frames Per Second: " << info.fps.ToDouble() << " (" << info.fps.num << "/" << info.fps.den << ")" << endl;
	cout << "--> Video Bit Rate: " << info.video_bit_rate/1000 << " kb/s" << endl;
	cout << "--> Pixel Ratio: " << info.pixel_ratio.ToDouble() << " (" << info.pixel_ratio.num << "/" << info.pixel_ratio.den << ")" << endl;
	cout << "--> Display Aspect Ratio: " << info.display_ratio.ToDouble() << " (" << info.display_ratio.num << "/" << info.display_ratio.den << ")" << endl;
	cout << "--> Video Codec: " << info.vcodec << endl;
	cout << "--> Video Length: " << info.video_length << " Frames" << endl;
	cout << "--> Video Stream Index: " << info.video_stream_index << endl;
	cout << "--> Video Timebase: " << info.video_timebase.ToDouble() << " (" << info.video_timebase.num << "/" << info.video_timebase.den << ")" << endl;
	cout << "--> Interlaced: " << info.interlaced_frame << endl;
	cout << "--> Interlaced: Top Field First: " << info.top_field_first << endl;
	cout << "----------------------------" << endl;
	cout << "----- Audio Attributes -----" << endl;
	cout << "----------------------------" << endl;
	cout << "--> Audio Codec: " << info.acodec << endl;
	cout << "--> Audio Bit Rate: " << info.audio_bit_rate/1000 << " kb/s" << endl;
	cout << "--> Sample Rate: " << info.sample_rate << " Hz" << endl;
	cout << "--> # of Channels: " << info.channels << endl;
	cout << "--> Channel Layout: " << info.channel_layout << endl;
	cout << "--> Audio Stream Index: " << info.audio_stream_index << endl;
	cout << "--> Audio Timebase: " << info.audio_timebase.ToDouble() << " (" << info.audio_timebase.num << "/" << info.audio_timebase.den << ")" << endl;
	cout << "----------------------------" << endl;
}

// Generate JSON string of this object
string WriterBase::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value WriterBase::JsonValue() {

	// Create root json object
	Json::Value root;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;
	root["has_single_image"] = info.has_single_image;
	root["duration"] = info.duration;
	stringstream filesize_stream;
	filesize_stream << info.file_size;
	root["file_size"] = filesize_stream.str();
	root["height"] = info.height;
	root["width"] = info.width;
	root["pixel_format"] = info.pixel_format;
	root["fps"] = Json::Value(Json::objectValue);
	root["fps"]["num"] = info.fps.num;
	root["fps"]["den"] = info.fps.den;
	root["video_bit_rate"] = info.video_bit_rate;
	root["pixel_ratio"] = Json::Value(Json::objectValue);
	root["pixel_ratio"]["num"] = info.pixel_ratio.num;
	root["pixel_ratio"]["den"] = info.pixel_ratio.den;
	root["display_ratio"] = Json::Value(Json::objectValue);
	root["display_ratio"]["num"] = info.display_ratio.num;
	root["display_ratio"]["den"] = info.display_ratio.den;
	root["vcodec"] = info.vcodec;
	stringstream video_length_stream;
	video_length_stream << info.video_length;
	root["video_length"] = video_length_stream.str();
	root["video_stream_index"] = info.video_stream_index;
	root["video_timebase"] = Json::Value(Json::objectValue);
	root["video_timebase"]["num"] = info.video_timebase.num;
	root["video_timebase"]["den"] = info.video_timebase.den;
	root["interlaced_frame"] = info.interlaced_frame;
	root["top_field_first"] = info.top_field_first;
	root["acodec"] = info.acodec;
	root["audio_bit_rate"] = info.audio_bit_rate;
	root["sample_rate"] = info.sample_rate;
	root["channels"] = info.channels;
	root["channel_layout"] = info.channel_layout;
	root["audio_stream_index"] = info.audio_stream_index;
	root["audio_timebase"] = Json::Value(Json::objectValue);
	root["audio_timebase"]["num"] = info.audio_timebase.num;
	root["audio_timebase"]["den"] = info.audio_timebase.den;

	// return JsonValue
	return root;
}

// Load JSON string into this object
void WriterBase::SetJson(string value) throw(InvalidJSON) {

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
void WriterBase::SetJsonValue(Json::Value root) {

	// Set data from Json (if key is found)
	if (!root["has_video"].isNull())
		info.has_video = root["has_video"].asBool();
	if (!root["has_audio"].isNull())
		info.has_audio = root["has_audio"].asBool();
	if (!root["has_single_image"].isNull())
		info.has_single_image = root["has_single_image"].asBool();
	if (!root["duration"].isNull())
		info.duration = root["duration"].asDouble();
	if (!root["file_size"].isNull())
		info.file_size = (long long) root["file_size"].asUInt();
	if (!root["height"].isNull())
		info.height = root["height"].asInt();
	if (!root["width"].isNull())
		info.width = root["width"].asInt();
	if (!root["pixel_format"].isNull())
		info.pixel_format = root["pixel_format"].asInt();
	if (!root["fps"].isNull() && root["fps"].isObject()) {
		if (!root["fps"]["num"].isNull())
			info.fps.num = root["fps"]["num"].asInt();
		if (!root["fps"]["den"].isNull())
		info.fps.den = root["fps"]["den"].asInt();
	}
	if (!root["video_bit_rate"].isNull())
		info.video_bit_rate = root["video_bit_rate"].asInt();
	if (!root["pixel_ratio"].isNull() && root["pixel_ratio"].isObject()) {
		if (!root["pixel_ratio"]["num"].isNull())
			info.pixel_ratio.num = root["pixel_ratio"]["num"].asInt();
		if (!root["pixel_ratio"]["den"].isNull())
			info.pixel_ratio.den = root["pixel_ratio"]["den"].asInt();
	}
	if (!root["display_ratio"].isNull() && root["display_ratio"].isObject()) {
		if (!root["display_ratio"]["num"].isNull())
			info.display_ratio.num = root["display_ratio"]["num"].asInt();
		if (!root["display_ratio"]["den"].isNull())
			info.display_ratio.den = root["display_ratio"]["den"].asInt();
	}
	if (!root["vcodec"].isNull())
		info.vcodec = root["vcodec"].asString();
	if (!root["video_length"].isNull())
		info.video_length = (long int) root["video_length"].asUInt();
	if (!root["video_stream_index"].isNull())
		info.video_stream_index = root["video_stream_index"].asInt();
	if (!root["video_timebase"].isNull() && root["video_timebase"].isObject()) {
		if (!root["video_timebase"]["num"].isNull())
			info.video_timebase.num = root["video_timebase"]["num"].asInt();
		if (!root["video_timebase"]["den"].isNull())
			info.video_timebase.den = root["video_timebase"]["den"].asInt();
	}
	if (!root["interlaced_frame"].isNull())
		info.interlaced_frame = root["interlaced_frame"].asBool();
	if (!root["top_field_first"].isNull())
		info.top_field_first = root["top_field_first"].asBool();
	if (!root["acodec"].isNull())
		info.acodec = root["acodec"].asString();

	if (!root["audio_bit_rate"].isNull())
		info.audio_bit_rate = root["audio_bit_rate"].asInt();
	if (!root["sample_rate"].isNull())
		info.sample_rate = root["sample_rate"].asInt();
	if (!root["channels"].isNull())
		info.channels = root["channels"].asInt();
	if (!root["channel_layout"].isNull())
		info.channel_layout = (ChannelLayout) root["channel_layout"].asInt();
	if (!root["audio_stream_index"].isNull())
		info.audio_stream_index = root["audio_stream_index"].asInt();
	if (!root["audio_timebase"].isNull() && root["audio_timebase"].isObject()) {
		if (!root["audio_timebase"]["num"].isNull())
			info.audio_timebase.num = root["audio_timebase"]["num"].asInt();
		if (!root["audio_timebase"]["den"].isNull())
			info.audio_timebase.den = root["audio_timebase"]["den"].asInt();
	}
}
