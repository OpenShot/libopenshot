
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

	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/test.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/test1.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/OpenShot_Now_In_3d.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/sintel-1024-stereo.mp4");
		openshot::FFmpegReader r("/home/jonathan/Videos/00119.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/piano.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Music/Army of Lovers/Crucified/Army of Lovers - Crucified [Single Version].mp3");
	//	openshot::FFmpegReader r("/home/jonathan/Documents/OpenShot Art/vimeo_banner.png");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/60fps.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/asdf.wdf");

		// Display debug info
		r.DisplayInfo();

		for (int frame = 1; frame < 3000; frame++)
		{
			Frame f = r.GetFrame(frame);
			f.Play();
			f.Display();
			f.DisplayWaveform(false);
		}

		//Player g;
		//g.SetReader(&r);
		//g.SetFrameCallback(&FrameReady);
		//g.Play();


		// Get a frame
		//Frame f = r.GetFrame(300);
//		f = r.GetFrame(301);
//		f = r.GetFrame(302);
//		f = r.GetFrame(303);
//		f.Display();
//		f.DisplayWaveform(false);
//		f.Play();

		r.Close();

		cout << "Successfully executed Main.cpp!" << endl;


	return 0;
}

