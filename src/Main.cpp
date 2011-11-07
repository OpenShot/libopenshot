
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
		openshot::FFmpegReader r("/home/jonathan/Videos/00212.MTS");
	//	openshot::FFmpegReader r("/home/jonathan/Music/beat.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Music/lonely island/B004YRCAOO_(disc_1)_09_-_Shy_Ronnie_2__Ronnie_&_Clyde_[Expli.mp3");
	//	openshot::FFmpegReader r("/home/jonathan/Pictures/Photo 3 - Sky.jpeg");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/60fps.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/asdf.wdf");

		// Display debug info
		r.DisplayInfo();

		for (int frame = 300; frame < 2000; frame++)
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
		//f = r.GetFrame(301);
		//f = r.GetFrame(302);
		//f = r.GetFrame(303);
		//f.Display();
		//f.Play();

		//f.Display();
		r.Close();

		cout << "Successfully executed Main.cpp!" << endl;


	return 0;
}

