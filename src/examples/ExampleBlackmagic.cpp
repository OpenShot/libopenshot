/**
 * @file
 * @brief Source file for Main_Blackmagic class (live greenscreen example app)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <memory>
#include "../../include/OpenShot.h"
#include <omp.h>
#include <time.h>

using namespace openshot;

int main(int argc, char *argv[])
{
	// Init datetime
	time_t rawtime;
	struct tm * timeinfo;

	/* TIMELINE ---------------- */
	Timeline t(1920, 1080, Fraction(30,1), 48000, 2, LAYOUT_STEREO);

	// Create background video
	ImageReader b1("/home/jonathan/Pictures/moon.jpg");
	ImageReader b2("/home/jonathan/Pictures/trees.jpg");
	ImageReader b3("/home/jonathan/Pictures/clouds.jpg");
	ImageReader b4("/home/jonathan/Pictures/minecraft.png");
	ImageReader b5("/home/jonathan/Pictures/colorpgg03.jpg");
	Clip c1(&b1);

	// Background counter
	int background_frame = 0;
	int background_id = 1;

	DecklinkReader dr(1, 11, 0, 2, 16);
	Clip c2(&dr);
	Clip c3(new ImageReader("/home/jonathan/Pictures/watermark.png"));

	// mask
	Clip c4(new ImageReader("/home/jonathan/Pictures/mask_small.png"));

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
	//c3.gravity = GRAVITY_BOTTOM;
	c3.scale = SCALE_NONE;
	c3.Layer(2);
	t.AddClip(&c3);

	// CLIP 4 (foreground image 2)
	c4.Position(0.0);
	c4.gravity = GRAVITY_TOP;
	c4.scale = SCALE_NONE;
	c4.Layer(3);
	//t.AddClip(&c4);

	// Decklink writer
	DecklinkWriter w(0, 11, 3, 2, 16);
	w.Open();

	// Loop through reader
	int x = 0;
	while (true)
	{
		std::shared_ptr<Frame> f = t.GetFrame(x);
		if (f)
		{
			if (x != 0 && x % 30 == 0)
			{
				cout << "30 frames... (" << abs(dr.GetCurrentFrameNumber() - x) << " diff)" << endl;

				if (x != 0 && x % 60 == 0)
				{
					time ( &rawtime );
					timeinfo = localtime ( &rawtime );

					stringstream timestamp;
					timestamp << asctime (timeinfo);

					stringstream filename;
					filename << "/home/jonathan/Pictures/screenshots/detailed/" << timestamp.str() << ".jpeg";
					f->Save(filename.str(), 1.0);
					stringstream filename_small;
					filename_small << "/home/jonathan/Pictures/screenshots/thumbs/" << timestamp.str() << ".jpeg";
					f->Save(filename_small.str(), 0.15);
				}
			}

			// Send current frame to BlackMagic
			w.WriteFrame(f);

			// Increment background frame #
			background_frame++;

			// Change background
			if (background_frame == 300)
			{
				background_frame = 0;
				switch (background_id)
				{
				case 1:
					c1.Reader(&b2);
					background_id = 2;
					break;
				case 2:
					c1.Reader(&b3);
					background_id = 3;
					break;
				case 3:
					c1.Reader(&b4);
					background_id = 4;
					break;
				case 4:
					c1.Reader(&b5);
					background_id = 5;
					break;
				case 5:
					c1.Reader(&b1);
					background_id = 1;
					break;
				}
			}


			//usleep(500 * 1);
			// Go to next frame on timeline
			if (abs(dr.GetCurrentFrameNumber() - x) > 40 || x == 90)
			{
				// Got behind... skip ahead some
				x = dr.GetCurrentFrameNumber();

				cout << "JUMPING AHEAD to " << x << ", background moved to " << (float(x) / 30.0f) << endl;
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
//	std::shared_ptr<Frame> f1 = r1.GetFrame(1);
//	r1.Close();
//	f1->TransparentColors("#8fa09a", 20.0);
//	f1->Display();
//	return 0;

//	ImageReader r2("/home/jonathan/Pictures/trees.jpg");
//	r2.Open();
//	std::shared_ptr<Frame> f2 = r2.GetFrame(1);
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
//		std::shared_ptr<Frame> f = dr.GetFrame(0);
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
