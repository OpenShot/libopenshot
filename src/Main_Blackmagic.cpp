
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <tr1/memory>
#include "../include/OpenShot.h"
#include <omp.h>

using namespace openshot;


int main(int argc, char *argv[])
{

	/* TIMELINE ---------------- */
	Timeline t(1920, 1080, Framerate(30,1), 48000, 2);

	// Add some clips
	Clip c1(new ImageReader("/home/jonathan/Pictures/moon.jpg"));
	//Clip c1(new FFmpegReader("/home/jonathan/Videos/sintel_trailer-720p.mp4"));
	Clip c2(new DecklinkReader(1, 11, 0, 2, 16));


	// CLIP 1 (background image)
	c1.Position(0.0);
	c1.scale = SCALE_NONE;
	//c1.End(30.0);
	c1.Layer(0);
	t.AddClip(&c1);

	// CLIP 2 (decklink live stream)
	c2.Position(0.0);
	c1.scale = SCALE_NONE;
	//c1.End(30.0);
	c2.Layer(1);
	t.AddClip(&c2);

	// Decklink writer
	DecklinkWriter w(0, 11, 3, 2, 16);
	w.Open();

	// Loop through reader
	int x = 0;
	while (true)
	{
		if (x % 30 == 0)
			cout << "30 frames..." << endl;

		tr1::shared_ptr<Frame> f = t.GetFrame(x);
		if (f)
		{
			//if (x % 30 == 0)
			//	t.GetFrame(0)->Display();

			w.WriteFrame(f);
			usleep(1000 * 1);

			x++;
		}
	}

	// Sleep
	sleep(4);




	// Image Reader
//	ImageReader r1("/home/jonathan/Pictures/Screenshot from 2013-02-10 15:06:38.png");
//	r1.Open();
//	tr1::shared_ptr<Frame> f1 = r1.GetFrame(1);
//	r1.Close();
//	f1->TransparentColors("#8fa09a", 20.0);
//	f1->Display();
//	return 0;

//	ImageReader r2("/home/jonathan/Pictures/trees.jpg");
//	r2.Open();
//	tr1::shared_ptr<Frame> f2 = r2.GetFrame(1);
//	r2.Close();

//	DecklinkReader dr(1, 11, 0, 2, 16);
//	dr.Open();
//
//	DecklinkWriter w(0, 11, 3, 2, 16);
//	w.Open();
//
//	// Loop through reader
//	int x = 0;
//	while (true)
//	{
//		if (x % 30 == 0)
//			cout << "30 frames..." << endl;
//
//		tr1::shared_ptr<Frame> f = dr.GetFrame(0);
//		if (f)
//		{
//			//f->Display();
//			w.WriteFrame(f);
//			usleep(1000 * 1);
//
//			x++;
//		}
//	}
//
//	// Sleep
//	sleep(4);
//
//	// Close writer
//	w.Close();

	return 0;
}
