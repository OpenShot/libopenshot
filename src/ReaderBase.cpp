/**
 * @file
 * @brief Source file for ReaderBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/ReaderBase.h"

using namespace openshot;

// Initialize the values of the FileInfo struct
void ReaderBase::InitFileInfo()
{
	info.has_video = false;
	info.has_audio = false;
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
	info.audio_stream_index = -1;
	info.audio_timebase = Fraction();
}

// Display file information
void ReaderBase::DisplayInfo() {
	cout << fixed << setprecision(2) << boolalpha;
	cout << "----------------------------" << endl;
	cout << "----- File Information -----" << endl;
	cout << "----------------------------" << endl;
	cout << "--> Has Video: " << info.has_video << endl;
	cout << "--> Has Audio: " << info.has_audio << endl;
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
	cout << "--> Audio Stream Index: " << info.audio_stream_index << endl;
	cout << "--> Audio Timebase: " << info.audio_timebase.ToDouble() << " (" << info.audio_timebase.num << "/" << info.audio_timebase.den << ")" << endl;
	cout << "----------------------------" << endl;
}

// Generate Json::JsonValue for this object
Json::Value ReaderBase::JsonValue() {

	// Create root json object
	Json::Value root;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;
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
	root["audio_stream_index"] = info.audio_stream_index;
	root["audio_timebase"] = Json::Value(Json::objectValue);
	root["audio_timebase"]["num"] = info.audio_timebase.num;
	root["audio_timebase"]["den"] = info.audio_timebase.den;

	// return JsonValue
	return root;
}

// Load Json::JsonValue into this object
void ReaderBase::SetJsonValue(Json::Value root) {

	// Set data from Json (if key is found)
	if (root["has_video"] != Json::nullValue)
		info.has_video = root["has_video"].asBool();
	if (root["has_audio"] != Json::nullValue)
		info.has_audio = root["has_audio"].asBool();
	if (root["duration"] != Json::nullValue)
		info.duration = root["duration"].asDouble();
	if (root["file_size"] != Json::nullValue)
		info.file_size = atoll(root["file_size"].asString().c_str());
	if (root["height"] != Json::nullValue)
		info.height = root["height"].asInt();
	if (root["width"] != Json::nullValue)
		info.width = root["width"].asInt();
	if (root["pixel_format"] != Json::nullValue)
		info.pixel_format = root["pixel_format"].asInt();
	if (root["fps"] != Json::nullValue) {
		info.fps.num = root["fps"]["num"].asInt();
		info.fps.den = root["fps"]["den"].asInt();
	}
	if (root["video_bit_rate"] != Json::nullValue)
		info.video_bit_rate = root["video_bit_rate"].asInt();
	if (root["pixel_ratio"] != Json::nullValue) {
		info.pixel_ratio.num = root["pixel_ratio"]["num"].asInt();
		info.pixel_ratio.den = root["pixel_ratio"]["den"].asInt();
	}
	if (root["display_ratio"] != Json::nullValue) {
		info.display_ratio.num = root["display_ratio"]["num"].asInt();
		info.display_ratio.den = root["display_ratio"]["den"].asInt();
	}
	if (root["vcodec"] != Json::nullValue)
		info.vcodec = root["vcodec"].asString();
	if (root["video_length"] != Json::nullValue)
		info.video_length = atoll(root["video_length"].asString().c_str());
	if (root["video_stream_index"] != Json::nullValue)
		info.video_stream_index = root["video_stream_index"].asInt();
	if (root["video_timebase"] != Json::nullValue) {
		info.video_timebase.num = root["video_timebase"]["num"].asInt();
		info.video_timebase.den = root["video_timebase"]["den"].asInt();
	}
	if (root["interlaced_frame"] != Json::nullValue)
		info.interlaced_frame = root["interlaced_frame"].asBool();
	if (root["top_field_first"] != Json::nullValue)
		info.top_field_first = root["top_field_first"].asBool();
	if (root["acodec"] != Json::nullValue)
		info.acodec = root["acodec"].asString();

	if (root["audio_bit_rate"] != Json::nullValue)
		info.audio_bit_rate = root["audio_bit_rate"].asInt();
	if (root["sample_rate"] != Json::nullValue)
		info.sample_rate = root["sample_rate"].asInt();
	if (root["channels"] != Json::nullValue)
		info.channels = root["channels"].asInt();
	if (root["audio_stream_index"] != Json::nullValue)
		info.audio_stream_index = root["audio_stream_index"].asInt();
	if (root["audio_timebase"] != Json::nullValue) {
		info.audio_timebase.num = root["audio_timebase"]["num"].asInt();
		info.audio_timebase.den = root["audio_timebase"]["den"].asInt();
	}
}
