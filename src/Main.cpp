/**
 * @file
 * @brief Source file for Main class (example app for libopenshot)
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

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <tr1/memory>
#include "../include/OpenShot.h"
#include "../include/Json.h"
#include <omp.h>
#include <qdir.h>

using namespace openshot;
using namespace tr1;

//void FrameReady(int number)
//{
//	cout << "Frame #: " << number << " is ready!" << endl;
//}

int main(int argc, char* argv[])
{
	// Create a empty clip
	Timeline t1(720, 480, Framerate(24,1), 44100, 2);
	DummyReader dr1(Framerate(24,1),720,480, 41000, 2, 10.0);

	// Change some properties
	Clip c1a(&dr1);
	c1a.Layer(1);
	c1a.Position(5.0);
	c1a.Start(5);
	c1a.End(10.5);
	t1.AddClip(&c1a);

	Clip c2a(&dr1);
	c2a.Layer(1);
	c2a.Position(5.0);
	c2a.Start(1);
	c2a.End(10.5);
	t1.AddClip(&c2a);

	Clip c3a(&dr1);
	c3a.Layer(1);
	c3a.Position(5.0);
	c3a.Start(3);
	c3a.End(10.5);
	t1.AddClip(&c3a);


	list<Clip*>::iterator clip_itr;
	list<Clip*> myClips = t1.Clips();
	for (clip_itr=myClips.begin(); clip_itr != myClips.end(); ++clip_itr)
	{
		// Get clip object from the iterator
		Clip *clip = (*clip_itr);

		// Open or Close this clip, based on if it's intersecting or not
		cout << clip->Start() << endl;
	}

	return 0;


	// Create a chunkwriter
//	FFmpegReader *r3 = new FFmpegReader("/home/jonathan/Videos/sintel_trailer-720p.mp4");
//	r3->DisplayInfo();
//	ChunkWriter cw1("/home/jonathan/apps/chunks/chunk1/", r3);
//	cw1.WriteFrame(r3, 1, r3->info.video_length - 45);
//	cw1.Close();
//	return 0;

	// FFmpegReader to test 1 part of a chunk
//	FFmpegReader *r10 = new FFmpegReader("/home/jonathan/apps/chunks/chunk1/final/000001.webm");
//	r10->enable_seek = false;
//	r10->Open();
////	r10->GetFrame(1)->Display();
////	r10->GetFrame(2)->Display();
////	r10->GetFrame(3)->Display();
////	r10->GetFrame(1)->Display();
//	for (int z1 = 20; z1 <= 24*3; z1++)
//		if (z1 >= 65)
//			r10->GetFrame(z1)->DisplayWaveform();
//	return 0;


	// Create a chunkreader
	cout << "Start Chunk Reader" << endl;
	ChunkReader cr1("/home/jonathan/apps/chunks/chunk1/", FINAL);
	cr1.DisplayInfo();
	cr1.Open();
	//for (int z1 = 70; z1 < 85; z1++)
	//	cr1.GetFrame(z1)->Display();
	//cr1.GetFrame(300)->Display();

	/* WRITER ---------------- */
	FFmpegWriter w9("/home/jonathan/fromchunks.webm");

	// Set options
	w9.SetAudioOptions(true, "libvorbis", cr1.info.sample_rate, cr1.info.channels, cr1.info.audio_bit_rate);
	w9.SetVideoOptions(true, cr1.info.vcodec, cr1.info.fps, cr1.info.width, cr1.info.height, cr1.info.pixel_ratio, false, false, cr1.info.video_bit_rate);

	// Prepare Streams
	w9.PrepareStreams();

	// Write header
	w9.WriteHeader();

	// Create a FFmpegWriter
	w9.WriteFrame(&cr1, 1, cr1.info.video_length - 48);

	// Write Footer
	w9.WriteTrailer();

	w9.Close();


	cout << "End Chunk Reader" << endl;
	return 0;

	// Qt Test Code
	if (argc == 2)
	{
		QDir dir(argv[1]);
		if (dir.exists()) {
			cout << "Yes, " << argv[1] << " exists!" << endl;
		}
	} else
		cout << "Not enough arguments!" << endl;


	// JSON Test Code
	Json::Value root;   // will contains the root value after parsing.
	Json::Reader reader;
	bool parsingSuccessful = reader.parse( "{\"age\":34, \"listy\" : [1, \"abc\" , 12.3, -4]}", root );
	if ( !parsingSuccessful )
	{
	    // report to the user the failure and their locations in the document.
	    std::cout  << "Failed to parse configuration\n"
	               << reader.getFormatedErrorMessages();
	}
	cout << root["listy"][2].asDouble() << endl;

	// Add a value
	root["final"] = 700;
	root["listy"].append(604);
	root.removeMember("age");

	// JSON Create new snippet
	Json::Value new_root;
	new_root["name"] = "jonathan";
	new_root["type_id"] = 23;
	new_root["amount"] = 100.50;
	new_root["listed"] = Json::Value(Json::arrayValue);
	new_root["listed"].append(0.5);
	new_root["listed"].append(1.5);
	new_root["listed"].append(Json::Value(Json::objectValue));
	new_root["listed"][2]["a"] = 1;
	new_root["listed"][2]["b"] = 2;
	cout << new_root << endl;

	return 0;

	// Chunk writer example
	FFmpegReader *r1 = new FFmpegReader("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	r1->Open();
	ChunkWriter cw("", r1);
	cw.WriteFrame(1, 600);

	return 0;



	TextReader r(720, 480, 10, 10, GRAVITY_TOP_RIGHT, "What's Up!", "Courier", 30, "Blue", "Black");
	r.Open();
	tr1::shared_ptr<Frame> f = r.GetFrame(1);
	f->Display();
	r.Close();

	return 0;



	/* TIMELINE ---------------- */
	Timeline t(1280, 720, Framerate(25,1), 44100, 2);


	// Add some clips
	Clip c1(new ImageReader("/home/jonathan/Desktop/Template2/video_background.jpg"));
	Clip c2(new FFmpegReader("/home/jonathan/Desktop/Template2/OpenShot Version 1-4-3.mp4"));
//	Clip c2(new FFmpegReader("/home/jonathan/Videos/sintel_trailer-480p.mp4"));
	Clip c3(new ImageReader("/home/jonathan/Desktop/Template2/logo.png"));
//	Clip c4(new ImageReader("/home/jonathan/Desktop/text3.png")); // audio
//	Clip c5(new ImageReader("/home/jonathan/Desktop/text1.png")); // color
//	Clip c6(new ImageReader("/home/jonathan/Desktop/text4.png")); // sub-pixel
//	Clip c7(new ImageReader("/home/jonathan/Desktop/text8.png")); // coming soon
//	Clip c10(new ImageReader("/home/jonathan/Desktop/text10.png")); // time mapping

	// CLIP 1 (background movie)
	c1.Position(0.0);
	c1.gravity = GRAVITY_LEFT;
	c1.scale = SCALE_CROP;
	c1.Layer(0);
	//c1.End(16.0);
	//c1.alpha.AddPoint(1, 0.0);
	//c1.alpha.AddPoint(500, 0.0);
	//c1.alpha.AddPoint(565, 1.0);
//	c1.time.AddPoint(1, 3096);
//	c1.time.AddPoint(180, 3276, LINEAR);
//	c1.time.AddPoint(181, 3276);
//	c1.time.AddPoint(361, 3096, LINEAR);
//	c1.time.AddPoint(362, 3096);
//	c1.time.AddPoint(722, 3276, LINEAR);
//	c1.time.AddPoint(723, 3276);
//	c1.time.AddPoint(903, 2916, LINEAR);
//	c1.time.AddPoint(1083, 3276, LINEAR);
	c1.time.PrintValues();
	//return 1;


	// CLIP 2 (wave form)
	c2.Position(0.0);
	c2.Layer(1);
	c2.Waveform(true);
//	c2.alpha.AddPoint(1, 1.0);
//	c2.alpha.AddPoint(150, 0.0);
//	c2.alpha.AddPoint(360, 0.0, LINEAR);
//	c2.alpha.AddPoint(384, 1.0);
	c2.volume.AddPoint(1, 1.0);
	c2.volume.AddPoint(900, 1.0, LINEAR);
	c2.volume.AddPoint(1000, 0.0, LINEAR);
//	c2.End(15.0);
	c2.wave_color.blue.AddPoint(1, 65000);
	c2.wave_color.blue.AddPoint(300, 0);
	c2.wave_color.red.AddPoint(1, 0);
	c2.wave_color.red.AddPoint(300, 65000);

	// CLIP 3 (watermark)
	c3.Layer(2);
//	c3.End(50);
	c3.gravity = GRAVITY_CENTER;
	c3.scale = SCALE_NONE;
	c3.alpha.AddPoint(1, 1.0);
	c3.alpha.AddPoint(150, 0.0);
//	c3.location_x.AddPoint(1, -5);
//	c3.location_y.AddPoint(1, -5);
//
//	// CLIP 4 (text about waveform)
//	c4.Layer(3);
//	c4.scale = SCALE_NONE;
//	c4.gravity = GRAVITY_CENTER;
//	c4.Position(1);
//	c4.alpha.AddPoint(1, 1.0);
//	c4.alpha.AddPoint(30, 0.0);
//	c4.alpha.AddPoint(100, 0.0);
//	c4.alpha.AddPoint(130, 1.0);
//	c4.End(5.5);
//
//	// CLIP 5 (text about colors)
//	c5.Layer(3);
//	c5.scale = SCALE_NONE;
//	c5.gravity = GRAVITY_CENTER;
//	c5.Position(16);
//	c5.alpha.AddPoint(1, 1.0);
//	c5.alpha.AddPoint(30, 0.0);
//	c5.alpha.AddPoint(100, 0.0);
//	c5.alpha.AddPoint(130, 1.0);
//	c5.End(5.5);
//
//	// CLIP 6 (text about sub-pixel)
//	c6.Layer(3);
//	c6.scale = SCALE_NONE;
//	c6.gravity = GRAVITY_LEFT;
//	c6.Position(24);
//	c6.alpha.AddPoint(1, 1.0);
//	c6.alpha.AddPoint(30, 0.0);
//	c6.alpha.AddPoint(100, 0.0);
//	c6.alpha.AddPoint(130, 1.0);
//	c6.location_x.AddPoint(1,0.05);
//	c6.location_x.AddPoint(130, 0.3);
//	c6.End(5.5);
//
//	// CLIP 7 (text about coming soon)
//	c7.Layer(3);
//	c7.scale = SCALE_NONE;
//	c7.gravity = GRAVITY_CENTER;
//	c7.Position(18.0);
//	c7.alpha.AddPoint(1, 1.0);
//	c7.alpha.AddPoint(30, 0.0);
//	c7.alpha.AddPoint(100, 0.0);
//	c7.alpha.AddPoint(130, 1.0);
//	c7.End(5.5);
//
//
//	// CLIP 10 (text about waveform)
//	c10.Layer(3);
//	c10.scale = SCALE_NONE;
//	c10.gravity = GRAVITY_CENTER;
//	c10.Position(1);
//	c10.alpha.AddPoint(1, 1.0);
//	c10.alpha.AddPoint(30, 0.0);
//	c10.alpha.AddPoint(100, 0.0);
//	c10.alpha.AddPoint(130, 1.0);
//	c10.End(5.5);

	// Add clips
	t.AddClip(&c1);
	t.AddClip(&c2);
	t.AddClip(&c3);
	//t.AddClip(&c4);
	//t.AddClip(&c5);
	//t.AddClip(&c6);
	//t.AddClip(&c7);
	//t.AddClip(&c10);
	/* ---------------- */



	/* WRITER ---------------- */
	FFmpegWriter w("/home/jonathan/output.mp4");

	// Set options
	//w.SetAudioOptions(true, "libvorbis", 48000, 2, 188000);
	w.SetAudioOptions(true, "libmp3lame", 44100, 2, 128000);
	//w.SetVideoOptions(true, "libvpx", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 30000000);
	w.SetVideoOptions(true, "mpeg4", Fraction(25,1), 1280, 720, Fraction(1,1), false, false, 30000000);

	// Prepare Streams
	w.PrepareStreams();

	// Write header
	w.WriteHeader();

	// Output stream info
	w.OutputStreamInfo();

	//for (int frame = 3096; frame <= 3276; frame++)
	for (int frame = 1; frame <= 1000; frame++)
	{
		tr1::shared_ptr<Frame> f = t.GetFrame(frame);
		if (f)
		{
			//if (frame >= 62)
			//f->DisplayWaveform();
			//f->AddOverlayNumber(frame);
			//f->Display();

			// Write frame
			cout << "queue frame " << frame << " (" << f->number << ", " << f << ")" << endl;
			w.WriteFrame(f);
		}
	}

	// Write Footer
	w.WriteTrailer();

	// Close writer & reader
	w.Close();

	// Close timeline
	t.Close();
	/* ---------------- */


	cout << "Successfully Finished Timeline DEMO" << endl;
	return 0;

}

