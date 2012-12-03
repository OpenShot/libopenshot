
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <tr1/memory>
#include "../include/OpenShot.h"
#include <omp.h>

using namespace openshot;
using namespace tr1;

void FrameReady(int number)
{
	cout << "Frame #: " << number << " is ready!" << endl;
}

int main()
{
//	// Create a reader
//	DummyReader r1(Framerate(24,1), 720, 480, 22000, 2, 5.0);
//
//	// Map frames
//	FrameMapper mapping(&r1, Framerate(60, 1), PULLDOWN_CLASSIC);
//	mapping.PrintMapping();
//
//	return 0;


//	//omp_set_num_threads(1);
//	//omp_set_nested(true);
//	//#pragma omp parallel
//	//{
//		//#pragma omp single
//		//{
//			cout << "Start reading JPEGS" << endl;
//			for (int x = 0; x <= 500; x++)
//			{
//				//#pragma omp task firstprivate(x)
//				//{
//					cout << x << endl;
//					ImageReader r("/home/jonathan/Desktop/TEMP/1.jpg");
//					tr1::shared_ptr<Frame> f(r.GetFrame(1));
//					//f->Save("/home/jonathan/Desktop/TEMP/1.jpg", 1.0);
//					r.Close();
//				//} // end omp task
//
//			} // end for loop
//
//		//} // end omp single
//	//} // end omp parallel
//	cout << "Done reading JPEGS" << endl;
//	return 0;


	// Create timeline
	Timeline t(624, 348, Framerate(24,1), 44100, 2);
	t.color.blue.AddPoint(1, 0);
	t.color.blue.AddPoint(300, 65000);

	// Add some clips
	//Clip c1(new FFmpegReader("/home/jonathan/Apps/videcho_site/media/user_files/videos/bd0bf442-3221-11e2-8bf6-001fd00ee3aa.webm"));
	//Clip c1(new FFmpegReader("/home/jonathan/Videos/Movie Music/02 - Shattered [Turn The Car Around] (Album Version).mp3"));
	FFmpegReader r1("/home/jonathan/Desktop/sintel.webm");
	Clip c1(new FFmpegReader("/home/jonathan/Videos/big-buck-bunny_trailer.webm"));
	Clip c2(new ImageReader("/home/jonathan/Desktop/Logo.png"));
	Clip c3(new FFmpegReader("/home/jonathan/Desktop/sintel.webm"));
	//Clip c3(new FFmpegReader("/home/jonathan/Videos/Movie Music/01 Whip It.mp3"));
	c1.Position(0.0);
	c1.gravity = GRAVITY_CENTER;
	c1.scale = SCALE_FIT;
	c1.End(20);
	c1.Layer(2);
	c1.volume.AddPoint(1, 0.0);
	c1.volume.AddPoint(100, 0.5);
	c1.volume.AddPoint(200, 0.0);
	c1.volume.AddPoint(300, 0.5);

	c1.Waveform(true);
//	c1.wave_color.blue.AddPoint(1, 0);
//	c1.wave_color.red.AddPoint(1, 65280);
//	c1.wave_color.red.AddPoint(300, 0);
//	c1.wave_color.green.AddPoint(1, 0);
//	c1.wave_color.green.AddPoint(300, 65280);

	//c1.scale_x.AddPoint(1, 0.5, LINEAR);
	//c1.scale_y.AddPoint(1, 0.5, LINEAR);

	c2.Position(0.0);
	c2.Layer(1);
	c2.gravity = GRAVITY_BOTTOM_RIGHT;
	c2.scale = SCALE_NONE;
	c2.alpha.AddPoint(1, 1);
	c2.alpha.AddPoint(120, 0);
	c2.location_x.AddPoint(1, -0.01);
	c2.location_x.AddPoint(300, -1);
	c2.location_y.AddPoint(1, -0.02);
	c2.End(20);

	c3.Layer(0);
	c3.End(20);
	c3.gravity = GRAVITY_CENTER;
	c3.volume.AddPoint(1, 0.5);
	//c3.scale_x.AddPoint(1, 0.1);
	//c3.scale_x.AddPoint(300, 2.0);
	//c3.scale_y.AddPoint(1, 0.1);
	//c3.scale_y.AddPoint(300, 2.0);

	//c2.scale_x.AddPoint(1, 1);
	//c2.scale_x.AddPoint(300, 3.5);

	//c2.scale_y.AddPoint(1, 1);
	//c2.scale_y.AddPoint(300, 3.5);

	//c1.scale_x.AddPoint(1, 1);
	//c1.scale_x.AddPoint(300, 1.5);

	//c1.scale_y.AddPoint(1, 1);
	//c1.scale_y.AddPoint(300, 1.5);

	//c1.alpha.AddPoint(1, 1);
	//c1.alpha.AddPoint(30, 0);

	//c2.alpha.AddPoint(1, 0);
	//c2.alpha.AddPoint(100, 1);
	//c2.alpha.AddPoint(200, 0);
	//c2.alpha.AddPoint(300, 1);

	//c2.location_x.AddPoint(1, 0);
	//c2.location_x.AddPoint(300, 1.0);

	//c2.location_y.AddPoint(1, 0);
	//c2.location_y.AddPoint(300, 1);

	//c2.rotation.AddPoint(1, 0);
	//c2.rotation.AddPoint(300, 360);


	// LINEAR Reverse
	//c1.time.AddPoint(1, 500, LINEAR);
	//c1.time.AddPoint(500, 1, LINEAR);

	// LINEAR Slow Reverse (sounds wavy, due to periodic repeated frames)
	//c1.time.AddPoint(1, 500, LINEAR);
	//c1.time.AddPoint(500, 100, LINEAR);

	// LINEAR Slow Reverse X2 (smooth)
	//c1.time.AddPoint(1, 500, LINEAR);
	//c1.time.AddPoint(500, 250, LINEAR);

	// LINEAR Slow Reverse X4 (smooth)
	//c1.time.AddPoint(1, 500, LINEAR);
	//c1.time.AddPoint(750, 250, LINEAR);

	// LINEAR Fast Reverse (sounds wavy, due to periodic repeated frames)
	//c1.time.AddPoint(1, 600, LINEAR);
	//c1.time.AddPoint(500, 1, LINEAR);

	// LINEAR Slow Forward
	//c1.time.AddPoint(1, 1000, LINEAR);
	//c1.time.AddPoint(500, 1, LINEAR);

//	// BEZIER Reverse
//	openshot::Point p1(1,500);
//	p1.handle_left = Coordinate(1,500);
//	p1.handle_right = Coordinate(250,500);
//
//	openshot::Point p2(500,100);
//	p1.handle_left = Coordinate(500,350);
//	p1.handle_right = Coordinate(500,100);
//
//	c1.time.AddPoint(p1);
//	c1.time.AddPoint(p2);

//	c1.time.AddPoint(1, 500, LINEAR);
//	c1.time.AddPoint(500, 1, LINEAR);
//	c1.time.AddPoint(1, 500, LINEAR);
//	c1.time.AddPoint(200, 200);
//	c1.time.AddPoint(500, 500, LINEAR);
//	c1.time.PrintValues();

	// Add clips
	t.AddClip(&c1);
	//t.AddClip(&c2);
	//t.AddClip(&c3);

	r1.Open();

	// Create a writer
	FFmpegWriter w("/home/jonathan/output.webm");
	r1.DisplayInfo();

	// Set options
	//w.SetAudioOptions(true, "libmp3lame", 44100, 2, 128000, false);
	w.SetAudioOptions(true, "libvorbis", 44100, 2, 128000);
	w.SetVideoOptions(true, "libvpx", Fraction(24,1), 624, 348, Fraction(1,1), false, false, 2000000);

	// Prepare Streams
	w.PrepareStreams();

	// Write header
	w.WriteHeader();

	// Output stream info
	w.OutputStreamInfo();

	for (int frame = 200; frame <= 400; frame++)
	{
		tr1::shared_ptr<Frame> f = r1.GetFrame(frame);
		if (f)
		{
			//if (frame >= 13)
			//	f->DisplayWaveform();

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

	cout << "Successfully Finished Timeline DEMO" << endl;
	return 0;




	// Create timeline
//	Framerate fps(30000,1000);
//	Timeline t(640, 480, fps);
//
//	// Add some clips
//	Clip c1("../../src/examples/test.mp4");
//	c1.Layer(2);
//	c1.Position(0.0);
//	c1.rotation.AddPoint(60, 360);
//
//	// Add clips
//	t.AddClip(&c1);
//
//	// Request frames
//	for (int x=1; x<=100; x++)
//		t.GetFrame(x);
//
//	// Close timeline
//	t.Close();
//
//	cout << "Successfully Finished Timeline DEMO" << endl;
//	return 0;



	//openshot::ImageReader i("/home/jonathan/Apps/videcho_site/media/logos/watermark3.png");
	//openshot::Frame* overlay = i.GetFrame(1);
	//i.DisplayInfo();

	//	openshot::FFmpegReader r("../../src/examples/test.mp4");
	//	openshot::FFmpegReader r("../../src/examples/test1.mp4");
	//	openshot::FFmpegReader r("../../src/examples/piano.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/big-buck-bunny_trailer.webm");

		openshot::FFmpegReader r("/home/jonathan/Videos/sintel-1024-stereo.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/OpenShot_Now_In_3d.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/piano.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Music/Army of Lovers/Crucified/Army of Lovers - Crucified [Single Version].mp3");
	//	openshot::FFmpegReader r("/home/jonathan/Documents/OpenShot Art/test.jpeg");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/60fps.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/asdf.wdf");



		// Display debug info
		r.Open();
		r.DisplayInfo();

//		for (int frame = 1; frame <= 300; frame++)
//		{
//			tr1::shared_ptr<Frame> f = r.GetFrame(frame);
//		}
//		return 0;





//		// Create a writer
//		FFmpegWriter w("/home/jonathan/output.webm");
//		w.DisplayInfo();
//
//		// Set options
//		w.SetAudioOptions(true, "libvorbis", 44100, 2, 128000, false);
//		//w.SetAudioOptions(true, "libmp3lame", 44100, 2, 128000, false);
//
//		w.SetVideoOptions(true, "libvpx", Fraction(24,1), 320, 240, Fraction(1,1), false, false, 2000000);
//		//w.SetVideoOptions(true, "libx264", Fraction(24,1), 320, 240, Fraction(1,1), false, false, 2000000);
//		//w.SetVideoOptions(true, "libtheora", Fraction(24,1), 320, 240, Fraction(1,1), false, false, 2000000);
//
//		// Prepare Streams
//		w.PrepareStreams();
//
//		// Set Options
////		w.SetOption(VIDEO_STREAM, "quality", "good");
////		w.SetOption(VIDEO_STREAM, "g", "120");
////		w.SetOption(VIDEO_STREAM, "qmin", "11");
////		w.SetOption(VIDEO_STREAM, "qmax", "51");
////		w.SetOption(VIDEO_STREAM, "profile", "0");
////		w.SetOption(VIDEO_STREAM, "speed", "0");
////		w.SetOption(VIDEO_STREAM, "level", "216");
////		w.SetOption(VIDEO_STREAM, "rc_lookahead", "16");
////		w.SetOption(VIDEO_STREAM, "rc_min_rate", "100000");
////		w.SetOption(VIDEO_STREAM, "rc_max_rate", "24000000");
////		w.SetOption(VIDEO_STREAM, "slices", "4");
////		w.SetOption(VIDEO_STREAM, "arnr_max_frames", "7");
////		w.SetOption(VIDEO_STREAM, "arnr_strength", "5");
////		w.SetOption(VIDEO_STREAM, "arnr_type", "3");
//
//		// Write header
//		w.WriteHeader();
//
//		// Output stream info
//		w.OutputStreamInfo();
//
//		//Frame *f = r.GetFrame(1);
//
//		//for (int frame = 131; frame >= 1; frame--)
//		for (int frame = 1; frame <= 300; frame++)
//		{
//			tr1::shared_ptr<Frame> f = r.GetFrame(frame);
//			//f->AddOverlayNumber(0);
//			//f->Display();
//
//			// Write frame
//			cout << "queue frame " << frame << endl;
//			w.WriteFrame(f);
//		}
//
//		// Write Footer
//		w.WriteTrailer();
//
//		// Close writer & reader
//		w.Close();
//		r.Close();


		cout << "Successfully executed Main.cpp!" << endl;

	return 0;
}

