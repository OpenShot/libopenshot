/**
 * @file
 * @brief Unit tests for openshot::Cache
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Cache_Default_Constructor)
{
	// Create cache object
	Cache c;

	// Loop 50 times
	for (int i = 0; i < 50; i++)
	{
		// Add blank frame to the cache
		tr1::shared_ptr<Frame> f(new Frame());
		c.Add(i, f);
	}

	CHECK_EQUAL(50, c.Count()); // Cache should have all frames, with no limit
	CHECK_EQUAL(0, c.GetMaxBytes()); // Max frames should default to 0
}

TEST(Cache_Max_Bytes_Constructor)
{
	// Create cache object (with a max of 5 previous items)
	Cache c(250 * 1024);

	// Loop 20 times
	for (int i = 30; i > 0; i--)
	{
		// Add blank frame to the cache
		tr1::shared_ptr<Frame> f(new Frame(i, 320, 240, "#000000"));
		c.Add(i, f);
	}

	// Cache should have all 20
	CHECK_EQUAL(20, c.Count());

	// Add 10 frames again
	for (int i = 10; i > 0; i--)
	{
		// Add blank frame to the cache
		tr1::shared_ptr<Frame> f(new Frame(i, 320, 240, "#000000"));
		c.Add(i, f);
	}

	// Count should be 20, since we're more frames than can be cached.
	CHECK_EQUAL(20, c.Count());

	// Check which items the cache kept
	CHECK_EQUAL(true, c.Exists(1));
	CHECK_EQUAL(true, c.Exists(10));
	CHECK_EQUAL(true, c.Exists(11));
	CHECK_EQUAL(true, c.Exists(19));
	CHECK_EQUAL(true, c.Exists(20));
	CHECK_EQUAL(false, c.Exists(21));
	CHECK_EQUAL(false, c.Exists(30));
}

TEST(Cache_Clear)
{
	// Create cache object
	Cache c(250 * 1024);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache
		tr1::shared_ptr<Frame> f(new Frame());
		c.Add(i, f);
	}

	// Cache should only have 10 items
	CHECK_EQUAL(10, c.Count());

	// Clear Cache
	c.Clear();

	// Cache should now have 0 items
	CHECK_EQUAL(0, c.Count());
}

TEST(Cache_Add_Duplicate_Frames)
{
	// Create cache object
	Cache c(250 * 1024);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache (each frame is #1)
		tr1::shared_ptr<Frame> f(new Frame());
		c.Add(1, f);
	}

	// Cache should only have 1 items (since all frames were frame #1)
	CHECK_EQUAL(1, c.Count());
}

TEST(Cache_Check_If_Frame_Exists)
{
	// Create cache object
	Cache c(250 * 1024);

	// Loop 5 times
	for (int i = 1; i < 6; i++)
	{
		// Add blank frame to the cache
		tr1::shared_ptr<Frame> f(new Frame());
		c.Add(i, f);
	}

	// Check if certain frames exists (only 1-5 exist)
	CHECK_EQUAL(false, c.Exists(0));
	CHECK_EQUAL(true, c.Exists(1));
	CHECK_EQUAL(true, c.Exists(2));
	CHECK_EQUAL(true, c.Exists(3));
	CHECK_EQUAL(true, c.Exists(4));
	CHECK_EQUAL(true, c.Exists(5));
	CHECK_EQUAL(false, c.Exists(6));
}

TEST(Cache_GetFrame)
{
	// Create cache object
	Cache c(250 * 1024);

	// Create 3 frames
	Frame *red = new Frame(1, 300, 300, "red");
	Frame *blue = new Frame(2, 400, 400, "blue");
	Frame *green = new Frame(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red->number, tr1::shared_ptr<Frame>(red));
	c.Add(blue->number, tr1::shared_ptr<Frame>(blue));
	c.Add(green->number, tr1::shared_ptr<Frame>(green));

	// Get frames
	CHECK_THROW(c.GetFrame(0), OutOfBoundsFrame);
	CHECK_THROW(c.GetFrame(4), OutOfBoundsFrame);

	// Check if certain frames exists (only 1-5 exist)
	CHECK_EQUAL(1, c.GetFrame(1)->number);
	CHECK_EQUAL(2, c.GetFrame(2)->number);
	CHECK_EQUAL(3, c.GetFrame(3)->number);
}

TEST(Cache_GetSmallest)
{
	// Create cache object (with a max of 10 items)
	Cache c(250 * 1024);

	// Create 3 frames
	Frame *red = new Frame(1, 300, 300, "red");
	Frame *blue = new Frame(2, 400, 400, "blue");
	Frame *green = new Frame(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red->number, tr1::shared_ptr<Frame>(red));
	c.Add(blue->number, tr1::shared_ptr<Frame>(blue));
	c.Add(green->number, tr1::shared_ptr<Frame>(green));

	// Check if frame 1 is the front
	CHECK_EQUAL(1, c.GetSmallestFrame()->number);

	// Check if frame 1 is STILL the front
	CHECK_EQUAL(1, c.GetSmallestFrame()->number);

	// Erase frame 1
	c.Remove(1);

	// Check if frame 2 is the front
	CHECK_EQUAL(2, c.GetSmallestFrame()->number);
}

TEST(Cache_Remove)
{
	// Create cache object (with a max of 10 items)
	Cache c(250 * 1024);

	// Create 3 frames
	Frame *red = new Frame(1, 300, 300, "red");
	Frame *blue = new Frame(2, 400, 400, "blue");
	Frame *green = new Frame(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red->number, tr1::shared_ptr<Frame>(red));
	c.Add(blue->number, tr1::shared_ptr<Frame>(blue));
	c.Add(green->number, tr1::shared_ptr<Frame>(green));

	// Check if count is 3
	CHECK_EQUAL(3, c.Count());

	// Check if frame 2 exists
	CHECK_EQUAL(true, c.Exists(2));

	// Remove frame 2
	c.Remove(2);

	// Check if frame 2 exists
	CHECK_EQUAL(false, c.Exists(2));

	// Check if count is 2
	CHECK_EQUAL(2, c.Count());

	// Remove frame 1
	c.Remove(1);

	// Check if frame 1 exists
	CHECK_EQUAL(false, c.Exists(1));

	// Check if count is 1
	CHECK_EQUAL(1, c.Count());
}

TEST(Cache_Set_Max_Bytes)
{
	// Create cache object
	Cache c;

	// Loop 20 times
	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		tr1::shared_ptr<Frame> f(new Frame());
		c.Add(i, f);
	}

	CHECK_EQUAL(0, c.GetMaxBytes()); // Cache defaults max frames to -1, unlimited frames

	// Set max frames
	c.SetMaxBytes(8 * 1024);
	CHECK_EQUAL(8 * 1024, c.GetMaxBytes());

	// Set max frames
	c.SetMaxBytes(4 * 1024);
	CHECK_EQUAL(4 * 1024, c.GetMaxBytes());
}
