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

