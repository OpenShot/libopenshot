
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
	//	openshot::FFmpegReader r("/home/jonathan/Videos/Version 1.1a.mp4");
		openshot::FFmpegReader r("/home/jonathan/Videos/sintel-1024-stereo.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/test.wav");
	//	openshot::FFmpegReader r("/home/jonathan/Music/Army of Lovers/Crucified/Army of Lovers - Crucified [Single Version].mp3");
	//	openshot::FFmpegReader r("/home/jonathan/Documents/OpenShot Art/bannertemplate.png");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/CMakeLists.txt");
	//	openshot::FFmpegReader r("/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/asdf.wdf");

		// Display debug info
		r.DisplayInfo();

		Player g;
		g.SetReader(&r);
		//g.SetFrameCallback(&FrameReady);
		g.Play();

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

