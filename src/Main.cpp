/**
 * @file
 * @brief Source file for Main class (example app for libopenshot)
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

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <tr1/memory>
#include "../include/OpenShot.h"
#include "../include/Json.h"
#include <omp.h>
#include <stdlib.h>
#include <time.h>


using namespace openshot;
using namespace tr1;

int main(int argc, char* argv[])
{

	// Reader
	FFmpegReader r9("/home/jonathan/Videos/sintel-1024-surround.mp4");
	r9.Open();
	//r9.info.has_audio = false;
	//r9.enable_seek = false;
	//r9.debug = true;

	/* WRITER ---------------- */
	//FFmpegWriter w9("/home/jonathan/output.webm");
	//w9.debug = true;
	ImageWriter w9("/home/jonathan/output.gif");

	// Set options
	//w9.SetAudioOptions(true, "libvorbis", 48000, r9.info.channels, r9.info.channel_layout, 120000);
	//w9.SetVideoOptions(true, "libvpx", r9.info.fps, r9.info.width, r9.info.height, r9.info.pixel_ratio, false, false, 1500000);
	//w9.SetVideoOptions(true, "rawvideo", r9.info.fps, 400, 2, r9.info.pixel_ratio, false, false, 20000000);
	w9.SetVideoOptions("GIF", r9.info.fps, r9.info.width, r9.info.height, 70, 1, true);

	// Open writer
	w9.Open();

	// Prepare Streams
	//w9.PrepareStreams();

//	w9.SetOption(VIDEO_STREAM, "qmin", "2" );
//	w9.SetOption(VIDEO_STREAM, "qmax", "30" );
//	w9.SetOption(VIDEO_STREAM, "crf", "10" );
//	w9.SetOption(VIDEO_STREAM, "rc_min_rate", "2000000" );
//	w9.SetOption(VIDEO_STREAM, "rc_max_rate", "4000000" );
//	w9.SetOption(VIDEO_STREAM, "max_b_frames", "10" );

	// Write header
	//w9.WriteHeader();

	//r9.DisplayInfo();

	// 147000 frames, 28100 frames
	//for (int frame = 1; frame <= (r9.info.video_length - 1); frame++)
	for (int frame = 500; frame <= 530; frame++)
	//int frame = 1;
	//while (true)
	{
		//int frame_number = (rand() % 750) + 1;
		int frame_number = ( frame);

		//cout << "queue " << frame << " (frame: " << frame_number << ") ";
		tr1::shared_ptr<Frame> f = r9.GetFrame(frame_number);
		//cout << "(" << f->number << ", " << f << ")" << endl;
		//f->DisplayWaveform();
		//f->AddColor(r9.info.width, r9.info.height, "blue");
		w9.WriteFrame(f);

		//frame++;
	}

	cout << "done looping" << endl;

	// Write Footer
	//w9.WriteTrailer();

	// Close writer & reader
	w9.Close();

	// Close timeline
	r9.Close();
	/* ---------------- */
	cout << "happy ending" << endl;

	return 0;






	FFmpegReader sinelReader("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	//sinelReader.debug = true;
	sinelReader.Open();

	// init random #s
	//srand(time(NULL));

	// Seek test
	int x = 0;
	while (true) {
		x++;
		int frame_number = (rand() % 625) + 1;
		cout << "X: " << x << ", Frame: " << frame_number << endl;
		tr1::shared_ptr<Frame> f = sinelReader.GetFrame(frame_number);
		//f->AddOverlayNumber(frame_number);
		//f->Display();
		f->DisplayWaveform();

		//f->DisplayWaveform();
		//	sinelReader.debug = true;

		//if (x == 7655)
		//	break;
	}

	//cout << sinelReader.OutputDebugJSON() << endl;
	sinelReader.Close();
	return 0;


//	Timeline t1000(1280, 720, Fraction(24,1), 44100, 2);
//	t1000.SetJson("{\"width\": 1280, \"clips\": [{\"position\": 0, \"layer\": 4, \"gravity\": 4, \"reader\": {\"width\": 640, \"file_size\": \"10998\", \"video_stream_index\": -1, \"duration\": 86400, \"top_field_first\": true, \"pixel_format\": -1, \"type\": \"ImageReader\", \"pixel_ratio\": {\"num\": 1, \"den\": 1}, \"video_timebase\": {\"num\": 1, \"den\": 30}, \"audio_bit_rate\": 0, \"has_audio\": false, \"sample_rate\": 0, \"audio_stream_index\": -1, \"video_bit_rate\": 0, \"fps\": {\"num\": 30, \"den\": 1}, \"channels\": 0, \"vcodec\": \"Joint Photographic Experts Group JFIF format\", \"video_length\": \"2592000\", \"interlaced_frame\": false, \"path\": \"/home/jonathan/Pictures/100_0685 (copy).JPG\", \"height\": 360, \"audio_timebase\": {\"num\": 1, \"den\": 1}, \"display_ratio\": {\"num\": 16, \"den\": 9}, \"has_video\": true, \"acodec\": \"\"}, \"title\": \"40319877_640.jpg\", \"duration\": 86400, \"scale\": 1, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"volume\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"time\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"waveform\": false, \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"wave_color\": {\"blue\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 65280}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 28672}, \"interpolation\": 2}]}, \"red\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"id\": \"F8GFFDCHSB\", \"location_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"end\": 23, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"alpha\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/LEUJBK9QMI.png\", \"file_id\": \"LEUJBK9QMI\", \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"start\": 0, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"anchor\": 0, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"$$hashKey\": \"00Y\"}, {\"position\": 8.64, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/LEUJBK9QMI.png\", \"gravity\": 4, \"reader\": {\"width\": 640, \"pixel_ratio\": {\"num\": 1, \"den\": 1}, \"video_stream_index\": -1, \"duration\": 86400, \"video_length\": \"2592000\", \"pixel_format\": -1, \"audio_timebase\": {\"num\": 1, \"den\": 1}, \"file_size\": \"10998\", \"video_timebase\": {\"num\": 1, \"den\": 30}, \"audio_bit_rate\": 0, \"has_audio\": false, \"sample_rate\": 0, \"audio_stream_index\": -1, \"video_bit_rate\": 0, \"fps\": {\"num\": 30, \"den\": 1}, \"channels\": 0, \"vcodec\": \"Joint Photographic Experts Group JFIF format\", \"top_field_first\": true, \"interlaced_frame\": false, \"path\": \"/home/jonathan/Pictures/100_0685 (copy).JPG\", \"height\": 360, \"display_ratio\": {\"num\": 16, \"den\": 9}, \"has_video\": true, \"acodec\": \"\", \"type\": \"ImageReader\"}, \"title\": \"40319877_640.jpg\", \"duration\": 86400, \"scale\": 1, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"volume\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"time\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"waveform\": false, \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"wave_color\": {\"blue\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 65280}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 28672}, \"interpolation\": 2}]}, \"red\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"end\": 24, \"id\": \"CIKGBFTVVY\", \"location_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"alpha\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"layer\": 3, \"file_id\": \"LEUJBK9QMI\", \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"start\": 0, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"anchor\": 0, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"$$hashKey\": \"011\"}, {\"position\": 40.16, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/LEUJBK9QMI.png\", \"gravity\": 4, \"reader\": {\"width\": 640, \"pixel_ratio\": {\"num\": 1, \"den\": 1}, \"video_stream_index\": -1, \"duration\": 86400, \"video_length\": \"2592000\", \"pixel_format\": -1, \"audio_timebase\": {\"num\": 1, \"den\": 1}, \"file_size\": \"10998\", \"video_timebase\": {\"num\": 1, \"den\": 30}, \"audio_bit_rate\": 0, \"has_audio\": false, \"sample_rate\": 0, \"audio_stream_index\": -1, \"video_bit_rate\": 0, \"fps\": {\"num\": 30, \"den\": 1}, \"channels\": 0, \"vcodec\": \"Joint Photographic Experts Group JFIF format\", \"top_field_first\": true, \"interlaced_frame\": false, \"path\": \"/home/jonathan/Pictures/100_0685 (copy).JPG\", \"height\": 360, \"display_ratio\": {\"num\": 16, \"den\": 9}, \"has_video\": true, \"acodec\": \"\", \"type\": \"ImageReader\"}, \"title\": \"40319877_640.jpg\", \"duration\": 86400, \"scale\": 1, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"volume\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"time\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"waveform\": false, \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"wave_color\": {\"blue\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 65280}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 28672}, \"interpolation\": 2}]}, \"red\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"end\": 47, \"id\": \"HFCX8JEV29\", \"location_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"alpha\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"layer\": 4, \"file_id\": \"LEUJBK9QMI\", \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"start\": 0, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"anchor\": 0, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"$$hashKey\": \"01B\"}], \"fps\": 30, \"progress\": [[0, 30, \"rendering\"], [40, 50, \"complete\"], [100, 150, \"complete\"]], \"duration\": 600, \"scale\": 16, \"tick_pixels\": 100, \"settings\": {}, \"files\": [{\"width\": 640, \"path\": \"/home/jonathan/Pictures/100_0685 (copy).JPG\", \"file_size\": \"10998\", \"video_stream_index\": -1, \"duration\": 86400.0, \"top_field_first\": true, \"pixel_format\": -1, \"type\": \"ImageReader\", \"pixel_ratio\": {\"num\": 1, \"den\": 1}, \"video_timebase\": {\"num\": 1, \"den\": 30}, \"audio_bit_rate\": 0, \"has_audio\": false, \"sample_rate\": 0, \"audio_stream_index\": -1, \"video_bit_rate\": 0, \"fps\": {\"num\": 30, \"den\": 1}, \"channels\": 0, \"vcodec\": \"Joint Photographic Experts Group JFIF format\", \"video_length\": \"2592000\", \"interlaced_frame\": false, \"media_type\": \"image\", \"id\": \"LEUJBK9QMI\", \"acodec\": \"\", \"audio_timebase\": {\"num\": 1, \"den\": 1}, \"display_ratio\": {\"num\": 16, \"den\": 9}, \"has_video\": true, \"height\": 360}], \"playhead_position\": 0, \"markers\": [{\"location\": 16, \"icon\": \"yellow.png\"}, {\"location\": 120, \"icon\": \"green.png\"}, {\"location\": 300, \"icon\": \"red.png\"}, {\"location\": 10, \"icon\": \"purple.png\"}], \"height\": 720, \"layers\": [{\"y\": 0, \"number\": 4}, {\"y\": 0, \"number\": 3}, {\"y\": 0, \"number\": 2}, {\"y\": 0, \"number\": 1}, {\"y\": 0, \"number\": 0}]}");
//	t1000.GetFrame(0)->Display();
//	//t1000.GetFrame(0)->Thumbnail("/home/jonathan/output.png", 320, 180, "/home/jonathan/Downloads/mask.png", "/home/jonathan/Downloads/overlay.png", "", false);
//	t1000.GetFrame(0)->Thumbnail("/home/jonathan/output.png", 134, 88, "/home/jonathan/Downloads/mask.png", "/home/jonathan/Downloads/overlay.png", "#000", false);

//	t1000.ApplyJsonDiff("[{\"key\": [\"clips\", {\"id\": \"BMCWP7ACMR\"}], \"value\": {\"end\": 8, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"location_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"reader\": {\"acodec\": \"\", \"channels\": 0, \"video_timebase\": {\"den\": 30, \"num\": 1}, \"type\": \"ImageReader\", \"video_length\": \"2592000\", \"has_video\": true, \"video_bit_rate\": 0, \"display_ratio\": {\"den\": 79, \"num\": 100}, \"vcodec\": \"Portable Network Graphics\", \"audio_stream_index\": -1, \"top_field_first\": true, \"fps\": {\"den\": 1, \"num\": 30}, \"has_audio\": false, \"interlaced_frame\": false, \"sample_rate\": 0, \"file_size\": \"412980\", \"pixel_ratio\": {\"den\": 1, \"num\": 1}, \"video_stream_index\": -1, \"audio_timebase\": {\"den\": 1, \"num\": 1}, \"pixel_format\": -1, \"duration\": 86400, \"height\": 1975, \"path\": \"/home/jonathan/Downloads/openshot_studios_banner1.png\", \"audio_bit_rate\": 0, \"width\": 2500}, \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"gravity\": 4, \"id\": \"BMCWP7ACMR\", \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"position\": 0, \"layer\": 3, \"$$hashKey\": \"00J\", \"alpha\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"waveform\": false, \"time\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"wave_color\": {\"red\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 28672}, \"interpolation\": 2}]}, \"blue\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 65280}, \"interpolation\": 2}]}}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"start\": 0, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"scale\": 1, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"anchor\": 0, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/JJNH7JOX9M.png\", \"file_id\": \"JJNH7JOX9M\", \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"duration\": 86400, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"title\": \"openshot_studios_banner1.png\", \"volume\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}}, \"partial\": false, \"type\": \"update\"}]");
//	t1000.GetFrame(0)->Display();
//	t1000.ApplyJsonDiff("[{\"key\": [\"clips\", {\"id\": \"BMCWP7ACMR\"}], \"value\": {\"end\": 50, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"location_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"reader\": {\"acodec\": \"\", \"channels\": 0, \"video_timebase\": {\"den\": 30, \"num\": 1}, \"type\": \"ImageReader\", \"video_length\": \"2592000\", \"has_video\": true, \"video_bit_rate\": 0, \"display_ratio\": {\"den\": 79, \"num\": 100}, \"vcodec\": \"Portable Network Graphics\", \"audio_stream_index\": -1, \"top_field_first\": true, \"fps\": {\"den\": 1, \"num\": 30}, \"has_audio\": false, \"interlaced_frame\": false, \"sample_rate\": 0, \"file_size\": \"412980\", \"pixel_ratio\": {\"den\": 1, \"num\": 1}, \"video_stream_index\": -1, \"audio_timebase\": {\"den\": 1, \"num\": 1}, \"pixel_format\": -1, \"duration\": 86400, \"height\": 1975, \"path\": \"/home/jonathan/Downloads/openshot_studios_banner1.png\", \"audio_bit_rate\": 0, \"width\": 2500}, \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"gravity\": 4, \"id\": \"BMCWP7ACMR\", \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"position\": 0, \"layer\": 3, \"$$hashKey\": \"00J\", \"alpha\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"waveform\": false, \"time\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"wave_color\": {\"red\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 28672}, \"interpolation\": 2}]}, \"blue\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 65280}, \"interpolation\": 2}]}}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"start\": 0, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"scale\": 1, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"anchor\": 0, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/JJNH7JOX9M.png\", \"file_id\": \"JJNH7JOX9M\", \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"duration\": 86400, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"title\": \"openshot_studios_banner1.png\", \"volume\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}}, \"partial\": false, \"type\": \"update\"}]");
//	t1000.GetFrame(0)->Display();
//	t1000.ApplyJsonDiff("[{\"key\": [\"clips\"], \"value\": {\"end\": 8.0, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"location_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"reader\": {\"acodec\": \"\", \"channels\": 0, \"video_length\": \"2592000\", \"video_timebase\": {\"den\": 30, \"num\": 1}, \"type\": \"ImageReader\", \"fps\": {\"den\": 1, \"num\": 30}, \"interlaced_frame\": false, \"video_bit_rate\": 0, \"display_ratio\": {\"den\": 1, \"num\": 1}, \"vcodec\": \"Portable Network Graphics\", \"audio_stream_index\": -1, \"top_field_first\": true, \"has_audio\": false, \"has_video\": true, \"sample_rate\": 0, \"file_size\": \"544426\", \"pixel_ratio\": {\"den\": 1, \"num\": 1}, \"video_stream_index\": -1, \"audio_timebase\": {\"den\": 1, \"num\": 1}, \"pixel_format\": -1, \"duration\": 86400.0, \"height\": 1347, \"path\": \"/home/jonathan/Downloads/OSlogo.png\", \"audio_bit_rate\": 0, \"width\": 1347}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"gravity\": 4, \"id\": \"0EW6OJW1N9\", \"title\": \"OSlogo.png\", \"file_id\": \"1YD3C3IZHX\", \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"position\": 0.0, \"layer\": 0, \"wave_color\": {\"red\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 28672.0}, \"interpolation\": 2}]}, \"blue\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 65280.0}, \"interpolation\": 2}]}}, \"alpha\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"waveform\": false, \"time\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 0.0}, \"interpolation\": 2}]}, \"start\": 0.0, \"scale\": 1, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"anchor\": 0, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/1YD3C3IZHX.png\", \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 1.0}, \"interpolation\": 2}]}, \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 1.0}, \"interpolation\": 2}]}, \"duration\": 86400.0, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": -1.0}, \"interpolation\": 2}]}, \"volume\": {\"Points\": [{\"co\": {\"X\": 0.0, \"Y\": 1.0}, \"interpolation\": 2}]}}, \"partial\": false, \"type\": \"insert\"}]");
//	t1000.GetFrame(0)->Display();
//	t1000.ApplyJsonDiff("[{\"key\": [\"clips\", {\"id\": \"0EW6OJW1N9\"}], \"value\": {\"end\": 8, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"location_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"reader\": {\"acodec\": \"\", \"channels\": 0, \"video_timebase\": {\"den\": 30, \"num\": 1}, \"type\": \"ImageReader\", \"video_length\": \"2592000\", \"has_video\": true, \"video_bit_rate\": 0, \"display_ratio\": {\"den\": 1, \"num\": 1}, \"vcodec\": \"Portable Network Graphics\", \"audio_stream_index\": -1, \"top_field_first\": true, \"fps\": {\"den\": 1, \"num\": 30}, \"has_audio\": false, \"interlaced_frame\": false, \"sample_rate\": 0, \"file_size\": \"544426\", \"pixel_ratio\": {\"den\": 1, \"num\": 1}, \"video_stream_index\": -1, \"audio_timebase\": {\"den\": 1, \"num\": 1}, \"pixel_format\": -1, \"duration\": 86400, \"height\": 1347, \"path\": \"/home/jonathan/Downloads/OSlogo.png\", \"audio_bit_rate\": 0, \"width\": 1347}, \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"gravity\": 4, \"id\": \"0EW6OJW1N9\", \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"position\": 0, \"layer\": 4, \"$$hashKey\": \"00Q\", \"alpha\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"waveform\": false, \"time\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"wave_color\": {\"red\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 28672}, \"interpolation\": 2}]}, \"blue\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 65280}, \"interpolation\": 2}]}}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"start\": 0, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"scale\": 1, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"anchor\": 0, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/1YD3C3IZHX.png\", \"file_id\": \"1YD3C3IZHX\", \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"duration\": 86400, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"title\": \"OSlogo.png\", \"volume\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}}, \"partial\": false, \"type\": \"update\"}]");
//	t1000.GetFrame(0)->Display();
//	t1000.ApplyJsonDiff("[{\"key\": [\"clips\", {\"id\": \"0EW6OJW1N9\"}], \"value\": {\"end\": 35, \"perspective_c1_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c1_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"location_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"location_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"reader\": {\"acodec\": \"\", \"channels\": 0, \"video_timebase\": {\"den\": 30, \"num\": 1}, \"type\": \"ImageReader\", \"video_length\": \"2592000\", \"has_video\": true, \"video_bit_rate\": 0, \"display_ratio\": {\"den\": 1, \"num\": 1}, \"vcodec\": \"Portable Network Graphics\", \"audio_stream_index\": -1, \"top_field_first\": true, \"fps\": {\"den\": 1, \"num\": 30}, \"has_audio\": false, \"interlaced_frame\": false, \"sample_rate\": 0, \"file_size\": \"544426\", \"pixel_ratio\": {\"den\": 1, \"num\": 1}, \"video_stream_index\": -1, \"audio_timebase\": {\"den\": 1, \"num\": 1}, \"pixel_format\": -1, \"duration\": 86400, \"height\": 1347, \"path\": \"/home/jonathan/Downloads/OSlogo.png\", \"audio_bit_rate\": 0, \"width\": 1347}, \"crop_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_width\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"gravity\": 4, \"id\": \"0EW6OJW1N9\", \"scale_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"shear_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"shear_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"position\": 0, \"layer\": 4, \"$$hashKey\": \"00Q\", \"alpha\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"rotation\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"waveform\": false, \"time\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"crop_height\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c4_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"wave_color\": {\"red\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"green\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 28672}, \"interpolation\": 2}]}, \"blue\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 65280}, \"interpolation\": 2}]}}, \"crop_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 0}, \"interpolation\": 2}]}, \"start\": 0, \"perspective_c3_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"scale\": 1, \"perspective_c2_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"perspective_c2_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"anchor\": 0, \"image\": \"/home/jonathan/.openshot_qt/thumbnail/1YD3C3IZHX.png\", \"file_id\": \"1YD3C3IZHX\", \"scale_x\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}, \"duration\": 86400, \"perspective_c3_y\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": -1}, \"interpolation\": 2}]}, \"title\": \"OSlogo.png\", \"volume\": {\"Points\": [{\"co\": {\"X\": 0, \"Y\": 1}, \"interpolation\": 2}]}}, \"partial\": false, \"type\": \"update\"}]");
//	t1000.GetFrame(0)->Display();

//	return 0;

	/*

	FFmpegReader sinelReader("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	sinelReader.Open();

	AudioReaderSource readerSource(&sinelReader, 1, 10000);
	for (int z = 0; z < 2000; z++) {
		// Get audio chunks
		int chunk_size = 750;
		juce::AudioSampleBuffer *master_buffer = new juce::AudioSampleBuffer(sinelReader.info.channels, chunk_size);
		master_buffer->clear();
		const AudioSourceChannelInfo info = {master_buffer, 0, chunk_size};

		// Get next audio block
		readerSource.getNextAudioBlock(info);

		// Delete buffer
		master_buffer->clear();
		delete master_buffer;
	}

	return 0;

	Profile p("/home/jonathan/Apps/openshot/openshot/profiles/atsc_1080p_25");
	return 0;

	Timeline t77(640, 480, Fraction(24,1), 44100, 2);
	t77.ApplyJsonDiff("[{\"type\":\"insert\",\"key\":[\"effects\",\"effect\"],\"value\":{\"end\":0,\"id\":\"e004\",\"layer\":0,\"order\":0,\"position\":0,\"start\":0,\"type\":\"Negate\"}}]");
	cout << t77.Json() << endl;
	t77.ApplyJsonDiff("[{\"type\":\"update\",\"key\":[\"effects\",\"effect\",{\"id\":\"e004\"}],\"value\":{\"order\":10.5,\"position\":11.6,\"start\":12.7}}]");
	cout << t77.Json() << endl;
	t77.ApplyJsonDiff("[{\"type\":\"delete\",\"key\":[\"effects\",\"effect\",{\"id\":\"e004\"}],\"value\":{}}]");
	cout << t77.Json() << endl;
	t77.ApplyJsonDiff("[{\"type\":\"insert\",\"key\":[\"color\"],\"value\":{\"blue\":{\"Points\":[{\"co\":{\"X\":0,\"Y\":30},\"interpolation\":2}]},\"green\":{\"Points\":[{\"co\":{\"X\":0,\"Y\":20},\"interpolation\":2}]},\"red\":{\"Points\":[{\"co\":{\"X\":0,\"Y\":10},\"interpolation\":2}]}}}]");
	cout << t77.Json() << endl;
	t77.ApplyJsonDiff("[{\"type\":\"delete\",\"key\":[\"color\"],\"value\":{}}]");
	cout << t77.Json() << endl;
	return 0;

	//FFmpegReader r2("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	//r2.Open();
	//cout << r2.Json() << endl;
	//r2.SetJson("{\"acodec\":\"\",\"audio_bit_rate\":0,\"audio_stream_index\":-1,\"audio_timebase\":{\"den\":1,\"num\":1},\"channels\":0,\"display_ratio\":{\"den\":9,\"num\":16},\"duration\":10.03333377838135,\"file_size\":\"208835074\",\"fps\":{\"den\":1,\"num\":30},\"has_audio\":false,\"has_video\":true,\"height\":1080,\"interlaced_frame\":false,\"path\":\"/home/jonathan/Videos/space_undulation_hd.mov\",\"pixel_format\":13,\"pixel_ratio\":{\"den\":72,\"num\":72},\"sample_rate\":0,\"top_field_first\":false,\"type\":\"FFmpegReader\",\"vcodec\":\"mjpeg\",\"video_bit_rate\":166513021,\"video_length\":\"301\",\"video_stream_index\":0,\"video_timebase\":{\"den\":30,\"num\":1},\"width\":1920}");
	Clip c1;
	c1.SetJson("{\"alpha\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":100,\"Y\":100,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":100,\"Y\":50,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":100,\"Y\":20,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"anchor\":0,\"crop_height\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"crop_width\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"crop_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"crop_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"end\":0,\"gravity\":4,\"layer\":0,\"location_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"location_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c1_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c2_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c3_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"perspective_c4_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":-1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"position\":0,\"rotation\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"scale\":1,\"scale_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"scale_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"shear_x\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"shear_y\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"start\":0,\"time\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"volume\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":1,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"wave_color\":{\"blue\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":65280,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":65280,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":65280,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"green\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":28672,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":28672,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":28672,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]},\"red\":{\"Auto_Handle_Percentage\":1,\"Points\":[{\"co\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_left\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_right\":{\"X\":0,\"Y\":0,\"delta\":0,\"increasing\":true,\"repeated\":{\"den\":1,\"num\":1}},\"handle_type\":0,\"interpolation\":0}]}},\"waveform\":false}");
	//c1.Reader(&r2);
	cout << c1.Json() << endl;
	//c1.Open();
	//c1.GetFrame(150)->Save("test.bmp", 1.0);
	return 0;

	*/


	// Image of interlaced frame
//	ImageReader ir("/home/jonathan/apps/libopenshot/src/examples/interlaced.png");
//	ir.Open();
//
//	// FrameMapper to de-interlace frame
//	//FrameMapper fm(&ir, Fraction(24,1), PULLDOWN_NONE);
//	//fm.DeInterlaceFrame(ir.GetFrame(1), true)->Display();
//	Deinterlace de(false);
//	de.GetFrame(ir.GetFrame(1), 1)->Display();
//
//
//	return 0;


	// Reader
	FFmpegReader r1("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	r1.Open();
	r1.DisplayInfo();
	r1.info.has_audio = false;
	//r1.enable_seek = true;

	// FrameMapper
	//FrameMapper r(&r1, Fraction(24,1), PULLDOWN_ADVANCED);
	//r.PrintMapping();

	/* WRITER ---------------- */
	FFmpegWriter w("/home/jonathan/output.mp4");

	// Set options
	//w.SetAudioOptions(true, "libvorbis", 48000, 2, 188000);
	w.SetAudioOptions(true, "libmp3lame", 44100, 1, LAYOUT_STEREO, 12800);
	w.SetVideoOptions(true, "mpeg4", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 30000000);
	//w.SetVideoOptions(true, "libmp3lame", openshot::Fraction(30,1), 720, 360, Fraction(1,1), false, false, 3000000);

	// Prepare Streams
	w.PrepareStreams();

	// Write header
	w.WriteHeader();

	// Output stream info
	w.OutputStreamInfo();

	//for (int frame = 3096; frame <= 3276; frame++)
	for (int frame = 1; frame <= 200; frame++)
	{
//		tr1::shared_ptr<Frame> f(new Frame(frame, 1280, 720, "#000000", 44100, 2));
//		if (frame % 2 == 0)
//			f->AddColor(1280, 720, "Yellow");
//		else
//			f->AddColor(1280, 720, "Black");
//
//		f->AddOverlayNumber(f->number);
//		cout << f->number << endl;
//		w.WriteFrame(f);

		tr1::shared_ptr<Frame> f = r1.GetFrame(frame);
		if (f)
		{
			//if (frame >= 250)
			//	f->DisplayWaveform();
			//f->AddOverlayNumber(frame);
			//f->Display();

			// Write frame
			//f->Display();
			cout << "queue frame " << frame << " (" << f->number << ", " << f << ")" << endl;
			w.WriteFrame(f);
		}
	}

	// Write Footer
	w.WriteTrailer();

	// Close writer & reader
	w.Close();

	// Close timeline
	r1.Close();
	/* ---------------- */


	cout << "Successfully Finished Timeline DEMO" << endl;
	return 0;

}

