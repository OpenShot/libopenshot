/**
 * @file
 * @brief Source file for ReaderBase class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <iostream>
#include <iomanip>
#include <sstream>

#include "ReaderBase.h"
#include "ClipBase.h"
#include "Frame.h"

#include "Json.h"


using namespace openshot;

/// Constructor for the base reader, where many things are initialized.
ReaderBase::ReaderBase()
{
	// Initialize info struct
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

	// Init parent clip
	clip = NULL;
}

// Display file information
void ReaderBase::DisplayInfo(std::ostream* out) {
	*out << std::fixed << std::setprecision(2) << std::boolalpha;
	*out << "----------------------------" << std::endl;
	*out << "----- File Information -----" << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "--> Has Video: " << info.has_video << std::endl;
	*out << "--> Has Audio: " << info.has_audio << std::endl;
	*out << "--> Has Single Image: " << info.has_single_image << std::endl;
	*out << "--> Duration: " << info.duration << " Seconds" << std::endl;
	*out << "--> File Size: " << double(info.file_size) / 1024 / 1024 << " MB" << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "----- Video Attributes -----" << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "--> Width: " << info.width << std::endl;
	*out << "--> Height: " << info.height << std::endl;
	*out << "--> Pixel Format: " << info.pixel_format << std::endl;
	*out << "--> Frames Per Second: " << info.fps.ToDouble() << " (" << info.fps.num << "/" << info.fps.den << ")" << std::endl;
	*out << "--> Video Bit Rate: " << info.video_bit_rate/1000 << " kb/s" << std::endl;
	*out << "--> Pixel Ratio: " << info.pixel_ratio.ToDouble() << " (" << info.pixel_ratio.num << "/" << info.pixel_ratio.den << ")" << std::endl;
	*out << "--> Display Aspect Ratio: " << info.display_ratio.ToDouble() << " (" << info.display_ratio.num << "/" << info.display_ratio.den << ")" << std::endl;
	*out << "--> Video Codec: " << info.vcodec << std::endl;
	*out << "--> Video Length: " << info.video_length << " Frames" << std::endl;
	*out << "--> Video Stream Index: " << info.video_stream_index << std::endl;
	*out << "--> Video Timebase: " << info.video_timebase.ToDouble() << " (" << info.video_timebase.num << "/" << info.video_timebase.den << ")" << std::endl;
	*out << "--> Interlaced: " << info.interlaced_frame << std::endl;
	*out << "--> Interlaced: Top Field First: " << info.top_field_first << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "----- Audio Attributes -----" << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "--> Audio Codec: " << info.acodec << std::endl;
	*out << "--> Audio Bit Rate: " << info.audio_bit_rate/1000 << " kb/s" << std::endl;
	*out << "--> Sample Rate: " << info.sample_rate << " Hz" << std::endl;
	*out << "--> # of Channels: " << info.channels << std::endl;
	*out << "--> Channel Layout: " << info.channel_layout << std::endl;
	*out << "--> Audio Stream Index: " << info.audio_stream_index << std::endl;
	*out << "--> Audio Timebase: " << info.audio_timebase.ToDouble() << " (" << info.audio_timebase.num << "/" << info.audio_timebase.den << ")" << std::endl;
	*out << "----------------------------" << std::endl;
	*out << "--------- Metadata ---------" << std::endl;
	*out << "----------------------------" << std::endl;

	// Iterate through metadata
	for (auto it : info.metadata)
		*out << "--> " << it.first << ": " << it.second << std::endl;
}

// Generate Json::Value for this object
Json::Value ReaderBase::JsonValue() const {

	// Create root json object
	Json::Value root;
	root["has_video"] = info.has_video;
	root["has_audio"] = info.has_audio;
	root["has_single_image"] = info.has_single_image;
	root["duration"] = info.duration;
	std::stringstream filesize_stream;
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
	std::stringstream video_length_stream;
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

	// Append metadata map
	root["metadata"] = Json::Value(Json::objectValue);

	for (const auto it : info.metadata)
		root["metadata"][it.first] = it.second;

	// return JsonValue
	return root;
}

// Load Json::Value into this object
void ReaderBase::SetJsonValue(const Json::Value root) {

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
		info.file_size = std::stoll(root["file_size"].asString());
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
		info.video_length = std::stoll(root["video_length"].asString());
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
	if (!root["metadata"].isNull() && root["metadata"].isObject()) {
		for( Json::Value::const_iterator itr = root["metadata"].begin() ; itr != root["metadata"].end() ; itr++ ) {
			std::string key = itr.key().asString();
			info.metadata[key] = root["metadata"][key].asString();
		}
	}
}

/// Parent clip object of this reader (which can be unparented and NULL)
openshot::ClipBase* ReaderBase::ParentClip() {
	return clip;
}

/// Set parent clip object of this reader
void ReaderBase::ParentClip(openshot::ClipBase* new_clip) {
	clip = new_clip;
}
