
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

	// Create background video
	FFmpegReader background_massive_warp("/home/jonathan/Videos/massive_warp_hd/%06d.tif");
	background_massive_warp.info.fps = Fraction(30,1);
	background_massive_warp.info.video_timebase = Fraction(1,30);
	background_massive_warp.final_cache.SetMaxBytes(35 * 1920 * 1080 * 4 + (44100 * 2 * 4));
	background_massive_warp.enable_seek = false;
//	background_massive_warp.Open();
//	for (int z = 1; z < 300; z++)
//	{
//		cout << "caching background frame: " << z << endl;
//		background_massive_warp.GetFrame(z);
//	}



	// Load 1st background
	int background_number = 1;
	int background_frame = 1;
	int background_repeat = 0;
	Clip c1(&background_massive_warp);

	DecklinkReader dr(1, 11, 0, 2, 16);
	Clip c2(&dr);
	Clip c3(new ImageReader("/home/jonathan/Pictures/mask_small.png"));

	// CLIP 1 (background image)
	c1.Position(0.0);
	c1.scale = SCALE_NONE;
	c1.Layer(0);
	t.AddClip(&c1);

	// CLIP 2 (decklink live stream)
	c2.Position(0.0);
	c2.scale = SCALE_NONE;
	c2.Layer(1);
	t.AddClip(&c2);

	// CLIP 3 (foreground image 1)
	c3.Position(0.0);
	c3.gravity = GRAVITY_TOP;
	c3.scale = SCALE_NONE;
	c3.Layer(2);
	//c3.alpha.AddPoint(1,1, LINEAR);
	//c3.alpha.AddPoint(40,1, LINEAR);
	//c3.alpha.AddPoint(100,0);
	//t.AddClip(&c3);

	// Decklink writer
	DecklinkWriter w(0, 11, 3, 2, 16);
	w.Open();

	// Loop through reader
	int x = 0;
	while (true)
	{
		tr1::shared_ptr<Frame> f = t.GetFrame(x);
		if (f)
		{
			if (x != 0 && x % 30 == 0)
			{
				cout << "30 frames... (" << abs(dr.GetCurrentFrameNumber() - x) << " diff)" << endl;

				if (x != 0 && x % 60 == 0)
				{
					stringstream filename;
					filename << "/home/jonathan/Pictures/screenshots/frame_" << x << ".jpeg";
					f->Save(filename.str(), 1.0);
				}
			}

			// Send current frame to BlackMagic
			w.WriteFrame(f);

			// Increment background frame #
			background_frame++;

			// Move background (if the end is reached)
			if (background_frame == (c1.Reader()->info.video_length - 12))
			{
				cout << "-- Restart background video --" << endl;
				// Reset counter
				background_frame = 1;
				background_repeat++;

				// Change background (after 3 repeats)
//				if (background_repeat == 1)
//				{
//					switch (background_number)
//					{
//					case 1:
//						// Switch to 2
//						background_massive_warp.final_cache.Clear();
//						c1.Reader(&background_space_undulation);
//						background_number = 2;
//						background_repeat = 0;
//						break;
//
//					case 2:
//						// Switch to 3
//						background_space_undulation.final_cache.Clear();
//						c1.Reader(&background_tract_hd);
//						background_number = 3;
//						background_repeat = 0;
//						break;
//
//					case 3:
//						// Switch to 1
//						background_tract_hd.final_cache.Clear();
//						c1.Reader(&background_massive_warp);
//						background_number = 1;
//						background_repeat = 0;
//						break;
//					}
//				}

				// Move Background Clip
				c1.Position(float(x-1) / 30.0f);

				// Close and Re-Open Reader()
				background_massive_warp.final_cache.Clear();
				c1.End(0.0); // this will cause the clip.open() method to reset the end.
				c1.Close();
				c1.Open();
			}

			//usleep(500 * 1);

			// Go to next frame on timeline
			if (abs(dr.GetCurrentFrameNumber() - x) > 40)
			{
				// Got behind... skip ahead some
				x = dr.GetCurrentFrameNumber();

				cout << "JUMPING AHEAD to " << x << ", background moved to " << (float(x) / 30.0f) << endl;

				// Move Background Clip
				c1.Position(float(x-1) / 30.0f);
				// Close and Re-Open Reader()
				c1.Reader()->Close();
				c1.Reader()->Open();
			}
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
