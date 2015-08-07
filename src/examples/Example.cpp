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

	EffectInfo::Json();
	return 0;

//	FFmpegReader r110("/home/jonathan/apps/libopenshot/src/examples/piano-mono.wav");
//	r110.Open();
//
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

	FFmpegReader r9("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	r9.Open();
	r9.DisplayInfo();


	// Mapper
	//FrameMapper map(&r9, Fraction(24,1), PULLDOWN_NONE, 48000, 2, LAYOUT_STEREO);
	//map.DisplayInfo();
	//map.debug = true;
	//map.Open();

	/* WRITER ---------------- */
	FFmpegWriter w9("/home/jonathan/output-pops.webm");
	w9.debug = false;
	//ImageWriter w9("/home/jonathan/output.gif");

	// Set options
	//w9.SetVideoOptions(true, "libx264", r9.info.fps, 1024, 576, Fraction(1,1), false, false, 1000000);
	//w9.SetAudioOptions(true, "mp2", r9.info.sample_rate, r9.info.channels, r9.info.channel_layout, 64000);
	w9.SetAudioOptions(true, "libvorbis", r9.info.sample_rate, r9.info.channels, r9.info.channel_layout, 128000);
	w9.SetVideoOptions(true, "libvpx", r9.info.fps, 1024, 576, Fraction(1,1), false, false, 3000000);
	//w9.SetAudioOptions(true, "libmp3lame", 22050, t10.info.channels, t10.info.channel_layout, 120000);
	//w9.SetVideoOptions(true, "libx264", t10.info.fps, t10.info.width, t10.info.height, t10.info.pixel_ratio, false, false, 1500000);
	//w9.SetVideoOptions(true, "rawvideo", r9.info.fps, 400, 2, r9.info.pixel_ratio, false, false, 20000000);
	//w9.SetVideoOptions("GIF", r9.info.fps, r9.info.width, r9.info.height, 70, 1, true);

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
	//for (int z = 0; z < 2; z++)
	for (int frame = 1; frame <= 700; frame++)
	//int frame = 1;
	//while (true)
	{
		//int frame_number = (rand() % 750) + 1;
		int frame_number = frame;

		cout << "get " << frame << " (frame: " << frame_number << ") " << endl;
		tr1::shared_ptr<Frame> f = r9.GetFrame(frame_number);
		cout << "mapped frame channel layouts: " << f->ChannelsLayout() << endl;
		cout << "display it (" << f->number << ", " << f << ")" << endl;
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
