/**
 * @file
 * @brief Source file for Example Executable (example app for libopenshot)
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
#include <tr1/memory>
#include "../../include/OpenShot.h"

using namespace openshot;
using namespace tr1;


int main(int argc, char* argv[])
{

	// Create a timeline
	Timeline r9(640, 480, Fraction(30, 1), 44100, 2, LAYOUT_STEREO);
	r9.SetJson("{\"settings\": {}, \"clips\": [{\"perspective_c2_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"alpha\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 1.0}}]}, \"rotation\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}, \"location_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}, \"gravity\": 4, \"start\": 0, \"scale\": 1, \"scale_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 1.0}}]}, \"title\": \"MAH04194.MP4\", \"image\": \".openshot_qt\\\\thumbnail\\\\RW78V6EM7H.png\", \"perspective_c4_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"volume\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 1.0}}]}, \"duration\": 11.02933311462402, \"perspective_c1_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"effects\": [], \"waveform\": false, \"wave_color\": {\"green\": {\"Points\": [{\"handle_right\": {\"X\": 1.0, \"Y\": 123.0}, \"handle_type\": 0, \"interpolation\": 0, \"co\": {\"X\": 1.0, \"Y\": 123.0}, \"handle_left\": {\"X\": 1.0, \"Y\": 123.0}}]}, \"blue\": {\"Points\": [{\"handle_right\": {\"X\": 1.0, \"Y\": 255.0}, \"handle_type\": 0, \"interpolation\": 0, \"co\": {\"X\": 1.0, \"Y\": 255.0}, \"handle_left\": {\"X\": 1.0, \"Y\": 255.0}}]}, \"alpha\": {\"Points\": [{\"handle_right\": {\"X\": 1.0, \"Y\": 255.0}, \"handle_type\": 0, \"interpolation\": 0, \"co\": {\"X\": 1.0, \"Y\": 255.0}, \"handle_left\": {\"X\": 1.0, \"Y\": 255.0}}]}, \"red\": {\"Points\": [{\"handle_right\": {\"X\": 1.0, \"Y\": 0.0}, \"handle_type\": 0, \"interpolation\": 0, \"co\": {\"X\": 1.0, \"Y\": 0.0}, \"handle_left\": {\"X\": 1.0, \"Y\": 0.0}}]}}, \"layer\": 0, \"position\": 0, \"perspective_c4_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"anchor\": 0, \"perspective_c3_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"crop_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}, \"crop_width\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"crop_height\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"shear_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}, \"shear_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}, \"end\": 11.02933311462402, \"file_id\": \"RW78V6EM7H\", \"perspective_c1_y\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"id\": \"L9S44F9NPE\", \"perspective_c2_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"perspective_c3_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": -1.0}}]}, \"reader\": {\"sample_rate\": 48000, \"duration\": 11.02933311462402, \"interlaced_frame\": false, \"video_stream_index\": 0, \"acodec\": \"aac\", \"video_bit_rate\": 12176893, \"pixel_format\": 0, \"top_field_first\": true, \"file_size\": \"16804113\", \"type\": \"FFmpegReader\", \"has_video\": true, \"audio_timebase\": {\"num\": 1, \"den\": 48000}, \"path\": \"\\\\Users\\\\Jonathan\\\\Videos\\\\MAH04194.MP4\", \"fps\": {\"num\": 25, \"den\": 1}, \"channel_layout\": 3, \"audio_bit_rate\": 127391, \"vcodec\": \"h264\", \"display_ratio\": {\"num\": 16, \"den\": 9}, \"video_length\": \"276\", \"has_audio\": true, \"audio_stream_index\": 1, \"video_timebase\": {\"num\": 1, \"den\": 25000}, \"has_single_image\": false, \"width\": 1440, \"height\": 1080, \"channels\": 2, \"pixel_ratio\": {\"num\": 4, \"den\": 3}}, \"time\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}, \"scale_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 1.0}}]}, \"crop_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}, \"location_x\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1.0, \"Y\": 0.0}}]}}], \"id\": \"T0\", \"sample_rate\": 44100, \"duration\": 300, \"layers\": [{\"number\": 0, \"id\": \"L4\", \"label\": \"\", \"y\": 0}], \"fps\": {\"num\": 24, \"den\": 1}, \"export_path\": \"\", \"scale\": 16, \"channel_layout\": 3, \"version\": {\"openshot-qt\": \"2.0.5\", \"libopenshot\": \"0.0.9\"}, \"tick_pixels\": 100, \"files\": [{\"sample_rate\": 48000, \"duration\": 11.02933311462402, \"interlaced_frame\": false, \"media_type\": \"video\", \"acodec\": \"aac\", \"video_bit_rate\": 12176893, \"pixel_format\": 0, \"top_field_first\": true, \"file_size\": \"16804113\", \"channels\": 2, \"has_single_image\": false, \"has_video\": true, \"id\": \"RW78V6EM7H\", \"audio_timebase\": {\"num\": 1, \"den\": 48000}, \"path\": \"\\\\Users\\\\Jonathan\\\\Videos\\\\MAH04194.MP4\", \"fps\": {\"num\": 25, \"den\": 1}, \"channel_layout\": 3, \"audio_bit_rate\": 127391, \"vcodec\": \"h264\", \"display_ratio\": {\"num\": 16, \"den\": 9}, \"video_length\": \"276\", \"has_audio\": true, \"audio_stream_index\": 1, \"video_timebase\": {\"num\": 1, \"den\": 25000}, \"type\": \"FFmpegReader\", \"width\": 1440, \"height\": 1080, \"video_stream_index\": 0, \"pixel_ratio\": {\"num\": 4, \"den\": 3}}], \"playhead_position\": 0, \"width\": 1920, \"effects\": [{\"brightness\": {\"Points\": [{\"handle_right\": {\"X\": 96.5999984741211, \"Y\": 1}, \"handle_type\": 0, \"interpolation\": 0, \"co\": {\"X\": 1, \"Y\": 1}, \"handle_left\": {\"X\": 1, \"Y\": 1}}, {\"handle_right\": {\"X\": 240, \"Y\": -1}, \"handle_type\": 0, \"interpolation\": 0, \"co\": {\"X\": 263.04, \"Y\": -1}, \"handle_left\": {\"X\": 144.3999938964844, \"Y\": -1}}]}, \"end\": 10.96, \"contrast\": {\"Points\": [{\"interpolation\": 2, \"co\": {\"X\": 1, \"Y\": 3}}]}, \"layer\": 0, \"replace_image\": false, \"id\": \"YDKTU9SEM4\", \"start\": 0, \"position\": 0, \"reader\": {\"sample_rate\": 0, \"duration\": 86400.0, \"interlaced_frame\": false, \"acodec\": \"\", \"video_bit_rate\": 0, \"pixel_format\": -1, \"top_field_first\": true, \"file_size\": \"1658880\", \"channels\": 0, \"has_single_image\": true, \"has_video\": true, \"audio_timebase\": {\"num\": 1, \"den\": 1}, \"path\": \"C:\\\\Users\\\\Jonathan\\\\Apps\\\\openshot-qt-git\\\\src\\\\transitions\\\\common\\\\circle_out_to_in.svg\", \"fps\": {\"num\": 30, \"den\": 1}, \"channel_layout\": 4, \"audio_bit_rate\": 0, \"vcodec\": \"\", \"display_ratio\": {\"num\": 5, \"den\": 4}, \"video_length\": \"2592000\", \"has_audio\": false, \"audio_stream_index\": -1, \"video_timebase\": {\"num\": 1, \"den\": 30}, \"type\": \"QtImageReader\", \"width\": 720, \"height\": 576, \"video_stream_index\": -1, \"pixel_ratio\": {\"num\": 1, \"den\": 1}}, \"type\": \"Mask\", \"title\": \"Transition\"}], \"progress\": [], \"profile\": \"HD 1080p 24 fps\", \"markers\": [], \"height\": 1080, \"channels\": 2}");
	r9.debug = false;

	// Add clips
	//r9.AddClip(&clip_video);
	//r9.AddClip(&clip_overlay);

	// Open Timeline
	r9.Open();

	int frame_count = 1;
	while (true) {
		int frame_number = (rand() % 200) + 1;
		cout << frame_count << ": reading frame " << frame_number << endl;
		r9.GetFrame(frame_number);

		frame_count++;
//		if (frame_count == 500)
//			return 0;
	}


	cout << " --> 1" << endl;
	r9.GetFrame(1)->Save("pic1.png", 1.0);
	cout << " --> 500" << endl;
	r9.GetFrame(500);
	cout << "1034" << endl;
	r9.GetFrame(1034);
	cout << "1" << endl;
	r9.GetFrame(1);
	cout << "1200" << endl;
	r9.GetFrame(1200)->Save("pic2.png", 1.0);


	/* WRITER ---------------- */
	FFmpegWriter w("output1.webm");

	// Set options
	w.SetAudioOptions(true, "libvorbis", 44100, 2, LAYOUT_STEREO, 188000);
	w.SetVideoOptions(true, "libvpx", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 3000000);

	// Open writer
	w.Open();

	// Prepare Streams
//	w.PrepareStreams();
//
//	w.SetOption(VIDEO_STREAM, "qmin", "2" );
//	w.SetOption(VIDEO_STREAM, "qmax", "30" );
//	w.SetOption(VIDEO_STREAM, "crf", "10" );
//	w.SetOption(VIDEO_STREAM, "rc_min_rate", "2000000" );
//	w.SetOption(VIDEO_STREAM, "rc_max_rate", "4000000" );
//	w.SetOption(VIDEO_STREAM, "max_b_frames", "10" );
//
//	// Write header
//	w.WriteHeader();

	// Write some frames
	w.WriteFrame(&r9, 24, 50);

	// Close writer & reader
	w.Close();

	return 0;
//
//	FFmpegReader r110("/home/jonathan/Videos/PlaysTV/Team Fortress 2/2015_07_06_22_43_16-ses.mp4");
//	r110.Open();
////	r110.debug = false;
////	r110.DisplayInfo();
////	FrameMapper m110(&r110, Fraction(24,1), PULLDOWN_NONE, 48000, 2, LAYOUT_STEREO);
//
//	Timeline t110(1280, 720, Fraction(24,1), 48000, 2, LAYOUT_STEREO);
//	Clip c110("/home/jonathan/Videos/PlaysTV/Team Fortress 2/2015_07_06_22_43_16-ses.mp4");
//	c110.Position(1.0);
//	t110.AddClip(&c110);
//	t110.Open();
//
////	m110.GetFrame(100);
////	m110.GetFrame(85);
////	m110.GetFrame(85);
////	m110.GetFrame(86);
////	m110.GetFrame(86);
////	m110.GetFrame(86);
////	m110.GetFrame(86);
////	m110.GetFrame(87);
////	m110.GetFrame(87);
//
//
//	t110.GetFrame(1000);
////	r110.GetFrame(96);
////	r110.GetFrame(97);
////	r110.GetFrame(95);
////	r110.GetFrame(98);
////	r110.GetFrame(100);
////	r110.GetFrame(101);
////	r110.GetFrame(103);
//	return 0;

//	for (int y = 600; y < 700; y++) {
//		cout << y << endl;
//		int random_frame_number = rand() % 1000;
//		t110.GetFrame(y);
//	}

//	srand (time(NULL));
//	for (int z = 0; z <= 1; z++)
//	for (int y = 1000; y < 1300; y++) {
//		cout << " --> " << y << endl;
//		int random_frame_number = rand() % 1000;
//		t110.GetFrame(y);
//	}

//	FrameMapper m110(&r110, Fraction(24,1), PULLDOWN_NONE, 22050, 2, LAYOUT_STEREO);
//	m110.Open();
//
//	Clip c110(&m110);
//	c110.Open();
//
//	Timeline t10(1280, 720, Fraction(24,1), 22050, 2, LAYOUT_STEREO);
//	t10.debug = false;
//	//Clip c20("/home/jonathan/Pictures/DSC00660.JPG");
//	//c20.End(1000.0);
//	//c20.Layer(-1);
//	//c20.scale = SCALE_STRETCH;
//	//c20.rotation.AddPoint(1, 0.0);
//	//c20.rotation.AddPoint(1000, 360.0);
//	Clip c10("/home/jonathan/apps/libopenshot/src/examples/piano-mono.wav");
//	c10.volume.AddPoint(1, 0.0);
//	c10.volume.AddPoint(100, 1.0);
////	c10.time.AddPoint(1, 1);
////	c10.time.AddPoint(300, 900);
////	c10.time.AddPoint(600, 300);
////	c10.time.PrintValues();
//
//	//Color background((unsigned char)0, (unsigned char)255, (unsigned char)0, (unsigned char)0);
//	//background.red.AddPoint(1000, 255);
//	//background.green.AddPoint(1000, 0);
//	//t10.color = background;
//
//	Color black;
//	black.red = Keyframe(0);
//	black.green = Keyframe(0);
//	black.blue = Keyframe(0);
//
//	Keyframe brightness;
//	brightness.AddPoint(300, -1.0, BEZIER);
//	brightness.AddPoint(370, 0.5, BEZIER);
//	brightness.AddPoint(425, -0.5, BEZIER);
//	brightness.AddPoint(600, 1.0, BEZIER);
//
//	//Negate e;
//	//Deinterlace e(false);
//	//ChromaKey e(black, Keyframe(30));
//	//QtImageReader mask_reader("/home/jonathan/apps/openshot-qt/src/transitions/extra/big_cross_right_barr.png");
//	//QtImageReader mask_reader1("/home/jonathan/apps/openshot-qt/src/transitions/extra/big_barr.png");
//	//Mask e(&mask_reader, brightness, Keyframe(3.0));
//	//c10.AddEffect(&e);
//	//Mask e1(&mask_reader1, brightness, Keyframe(3.0));
//	//c10.AddEffect(&e1);
//
//	// add clip to timeline
//	t10.AddClip(&c10);
//	//t10.AddClip(&c20);
//	t10.Open();

//	FFmpegReader r9("/home/jonathan/Videos/sintel_trailer-720p.mp4");
//	r9.Open();
//	r9.DisplayInfo();


	// Mapper
	//FrameMapper map(&r9, Fraction(24,1), PULLDOWN_NONE, 48000, 2, LAYOUT_STEREO);
	//map.DisplayInfo();
	//map.debug = true;
	//map.Open();

	/* WRITER ---------------- */
	FFmpegWriter w9("C:\\Users\\Jonathan\\test-output.avi");
	w9.debug = false;
	//ImageWriter w9("/home/jonathan/output.gif");

	// Set options
	//w9.SetVideoOptions(true, "mpeg4", r9.info.fps, r9.info.width, r9.info.height, Fraction(1,1), false, false, 1000000);
	//w9.SetAudioOptions(true, "mp2", r9.info.sample_rate, r9.info.channels, r9.info.channel_layout, 64000);
	w9.SetVideoOptions(true, "libx264", r9.info.fps, r9.info.width, r9.info.height, Fraction(1,1), false, false, 1000000);
	w9.SetAudioOptions(true, "mp2", r9.info.sample_rate, r9.info.channels, r9.info.channel_layout, 64000);
	//w9.SetAudioOptions(true, "libvorbis", r9.info.sample_rate, r9.info.channels, r9.info.channel_layout, 128000);
	//w9.SetVideoOptions(true, "libvpx", r9.info.fps, r9.info.width, r9.info.height, Fraction(1,1), false, false, 3000000);
	//w9.SetAudioOptions(true, "libmp3lame", 22050, r9.info.channels, r9.info.channel_layout, 120000);
	//w9.SetVideoOptions(true, "libx264", t10.info.fps, t10.info.width, t10.info.height, t10.info.pixel_ratio, false, false, 1500000);
	//w9.SetVideoOptions(true, "rawvideo", r9.info.fps, 400, 2, r9.info.pixel_ratio, false, false, 20000000);
	//w9.SetVideoOptions("GIF", r9.info.fps, r9.info.width, r9.info.height, 70, 1, true);

	// Open writer
	w9.Open();

	// Prepare Streams
	w9.PrepareStreams();

//	w9.SetOption(VIDEO_STREAM, "qmin", "2" );
//	w9.SetOption(VIDEO_STREAM, "qmax", "30" );
//	w9.SetOption(VIDEO_STREAM, "crf", "10" );
//	w9.SetOption(VIDEO_STREAM, "rc_min_rate", "2000000" );
//	w9.SetOption(VIDEO_STREAM, "rc_max_rate", "4000000" );
//	w9.SetOption(VIDEO_STREAM, "max_b_frames", "10" );

	// Write header
	w9.WriteHeader();
	//r9.DisplayInfo();

	// 147000 frames, 28100 frames
	//for (int frame = 1; frame <= (r9.info.video_length - 1); frame++)
	//for (int z = 0; z < 2; z++)
	for (long int frame = 500; frame <= 750; frame++)
	//int frame = 1;
	//while (true)
	{
		//int frame_number = (rand() % 750) + 1;
		int frame_number = frame;

		cout << "get " << frame << " (frame: " << frame_number << ") " << endl;
		tr1::shared_ptr<Frame> f = r9.GetFrame(frame_number);
		//cout << "mapped frame channel layouts: " << f->ChannelsLayout() << endl;
		//cout << "display it (" << f->number << ", " << f << ")" << endl;
		//r9.GetFrame(frame_number)->DisplayWaveform();
		//if (frame >= 495)
		//	f->DisplayWaveform();
		//f->Display();
		//f->Save("/home/jonathan/test.png", 1.0);
		//f->AddColor(r9.info.width, r9.info.height, "blue");
		w9.WriteFrame(f);

		//frame++;

		//if (frame >= 100)
		//	break;
	}

	cout << "done looping" << endl;

	// Write Footer
	//w9.WriteTrailer();

	// Close writer & reader
	w9.Close();

	// Close timeline
	r9.Close();
	//t10.Close();
	/* ---------------- */
	cout << "happy ending" << endl;

	return 0;





}

//int main(int argc, char* argv[])
//{
//	for (int z = 0; z<10; z++)
//		main2();
//}
