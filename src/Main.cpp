
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
		w.SetAudioOptions(true, "libvorbis", 44100, 2, 128000);
		w.SetVideoOptions(true, "libvpx", Fraction(25, 1), 640, 360, Fraction(1,1), false, false, 2000000);

		// Prepare Streams
		w.PrepareStreams();

		// Set Options
		w.SetOption(VIDEO_STREAM, "quality", "good");
//		w.SetOption(VIDEO_STREAM, "g", "120");
//		w.SetOption(VIDEO_STREAM, "qmin", "11");
//		w.SetOption(VIDEO_STREAM, "qmax", "51");
//		w.SetOption(VIDEO_STREAM, "profile", "0");
//		w.SetOption(VIDEO_STREAM, "speed", "0");
//		w.SetOption(VIDEO_STREAM, "level", "216");
//		w.SetOption(VIDEO_STREAM, "rc_lookahead", "16");
//		w.SetOption(VIDEO_STREAM, "rc_min_rate", "100000");
//		w.SetOption(VIDEO_STREAM, "rc_max_rate", "24000000");
//		w.SetOption(VIDEO_STREAM, "slices", "4");
//		w.SetOption(VIDEO_STREAM, "arnr_max_frames", "7");
//		w.SetOption(VIDEO_STREAM, "arnr_strength", "5");
//		w.SetOption(VIDEO_STREAM, "arnr_type", "3");

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

