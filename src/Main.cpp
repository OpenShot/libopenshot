
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
	//	openshot::FFmpegReader r("../../src/examples/test.mp4");
	//	openshot::FFmpegReader r("../../src/examples/test1.mp4");
	//	openshot::FFmpegReader r("../../src/examples/piano.wav");
		openshot::FFmpegReader r("/home/jonathan/Videos/sintel-1024-stereo.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/00001.mts");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/piano.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Music/Army of Lovers/Crucified/Army of Lovers - Crucified [Single Version].mp3");
	//	openshot::FFmpegReader r("/home/jonathan/Documents/OpenShot Art/test.jpeg");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/60fps.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/asdf.wdf");

		// Display debug info
		r.DisplayInfo();

		// Create a writer
		FFmpegWriter w("/home/jonathan/output.webm");
		w.DisplayInfo();

		// Set options
		w.SetVideoOptions(true, "libvpx", Fraction(24, 1), 640, 360, Fraction(1,1), false, false, 384000);
		w.SetAudioOptions(true, "libvorbis", 44100, 2, 128000);

		// Prepare Streams
		w.PrepareStreams();

		// Set Options
		//w.SetOption(VIDEO_STREAM, "quality", "good");
		w.SetOption(VIDEO_STREAM, "g", "120");
		w.SetOption(VIDEO_STREAM, "qmin", "10");
		w.SetOption(VIDEO_STREAM, "qmax", "42");
		w.SetOption(VIDEO_STREAM, "profile", "0");
		w.SetOption(VIDEO_STREAM, "level", "216");
		w.SetOption(VIDEO_STREAM, "rc_lookahead", "16");
		//w.SetOption(VIDEO_STREAM, "max_b_frames", "2");
		//w.SetOption(VIDEO_STREAM, "mb_decision", "2");

		// Write header
		w.WriteHeader();

		// Output stream info
		w.OutputStreamInfo();

		for (int frame = 1; frame <= 500; frame++)
		{
			Frame f = r.GetFrame(frame);

			// Write frame
			cout << "Write frame " << f.number << endl;
			w.WriteFrame(&f);
		}

		// Write Footer
		w.WriteTrailer();

		// Close writer & reader
		w.Close();
		r.Close();


		cout << "Successfully executed Main.cpp!" << endl;

	return 0;
}

