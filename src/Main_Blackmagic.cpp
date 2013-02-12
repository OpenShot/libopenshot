
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
	DecklinkReader dr(1, 11, 0, 2, 16);
	Clip c2(&dr);
	Clip c3(new ImageReader("/home/jonathan/Pictures/mask_small.png"));
	Clip c4(new ImageReader("/home/jonathan/Pictures/jason-mask.png"));


	// CLIP 1 (background image)
	c1.Position(0.0);
	c1.scale = SCALE_NONE;
	//c1.End(30.0);
	c1.Layer(0);
	t.AddClip(&c1);

	// CLIP 2 (decklink live stream)
	c2.Position(0.0);
	c2.scale = SCALE_NONE;
	//c1.End(30.0);
	c2.Layer(1);
	t.AddClip(&c2);

	// CLIP 3 (foreground image 1)
	c3.Position(0.0);
	c3.gravity = GRAVITY_TOP;
	c3.scale = SCALE_NONE;
	//c1.End(30.0);
	c3.Layer(2);
	//c3.alpha.AddPoint(1,1);
	//c3.alpha.AddPoint(60,0);
	t.AddClip(&c3);

	// CLIP 4 (foreground image 2)
	c4.Position(0.0);
	c4.gravity = GRAVITY_BOTTOM;
	c4.scale = SCALE_NONE;
	//c1.End(30.0);
	c4.Layer(3);
	t.AddClip(&c4);

	// Decklink writer
	DecklinkWriter w(0, 11, 3, 2, 16);
	w.Open();

	// Loop through reader
	int x = 0;
	while (true)
	{
		if (x != 0 && x % 30 == 0)
			cout << "30 frames..." << endl;

		tr1::shared_ptr<Frame> f = t.GetFrame(x);
		if (f)
		{
			// Send current frame to BlackMagic
			w.WriteFrame(f);

			// Sleep some
			//usleep(1000 * 1);

			// Go to next frame on timeline
			if (abs(dr.GetCurrentFrameNumber() - x) > 90)
				// Got behind... skip ahead some
				x = dr.GetCurrentFrameNumber() - 8;
			else
				// Go to the next frame
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
