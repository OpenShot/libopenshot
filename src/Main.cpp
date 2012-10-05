
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include "../include/OpenShot.h"

using namespace openshot;

void FrameReady(int number)
{
	cout << "Frame #: " << number << " is ready!" << endl;
}

int main()
{
	// Create timeline
	Framerate fps(30000,1000);
	Timeline t(640, 480, fps);

	// Add some clips
	Clip c1;
	c1.Position(10.0);

	Clip c2;
	c2.Position(0.0);

	Clip c3;
	c3.Position(2.5);

	Clip c4;
	c4.Position(40.4);

	Clip c5;
	c5.Position(0.1);

	t.AddClip(&c4);
	t.AddClip(&c2);
	t.AddClip(&c3);
	t.AddClip(&c1);
	t.AddClip(&c5);

	t.GetFrame(1);

	c1.Position(50.0);
	c2.Position(42.0);
	c3.Position(33.0);
	c4.Position(63.0);
	c5.Position(13.0);

	t.SortClips();
	t.GetFrame(1);

	// Create a clip
	Clip c("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	cout << "Position: " << c.Position() << endl;
	cout << "Start: " << c.Start() << endl;
	cout << "End: " << c.End() << endl;

	return 0;

	//openshot::ImageReader i("/home/jonathan/Apps/videcho_site/media/logos/watermark3.png");
	//openshot::Frame* overlay = i.GetFrame(1);
	//i.DisplayInfo();

	//	openshot::FFmpegReader r("../../src/examples/test.mp4");
	//	openshot::FFmpegReader r("../../src/examples/test1.mp4");
	//	openshot::FFmpegReader r("../../src/examples/piano.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/big-buck-bunny_trailer.webm");

	//	openshot::FFmpegReader r("/home/jonathan/Videos/sintel-1024-stereo.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/OpenShot_Now_In_3d.mp4");
		openshot::FFmpegReader r("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/piano.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Music/Army of Lovers/Crucified/Army of Lovers - Crucified [Single Version].mp3");
	//	openshot::FFmpegReader r("/home/jonathan/Documents/OpenShot Art/test.jpeg");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/60fps.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/asdf.wdf");

		// Display debug info
		r.DisplayInfo();

//		// Create a writer
//		FFmpegWriter w("/home/jonathan/output.webm");
//		w.DisplayInfo();
//
//		// Set options
//		w.SetAudioOptions(true, "libvorbis", 44100, 2, 128000, false);
//		w.SetVideoOptions(true, "libvpx", Fraction(24, 1), 640, 360, Fraction(1,1), false, false, 2000000);
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
//		for (int frame = 1; frame <= 1000; frame++)
//		{
//			Frame *f = r.GetFrame(frame);
//			//f->AddOverlay(overlay);
//
//			//if (f->number == 307 || f->number == 308 || f->number == 309 || f->number == 310)
//			//f->DisplayWaveform();
//
//			// Apply effect
//			//f->AddEffect("flip");
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
		r.Close();
		//i.Close();


		cout << "Successfully executed Main.cpp!" << endl;

	return 0;
}

