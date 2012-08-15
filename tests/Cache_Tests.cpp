#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(Cache_Default_Constructor)
{
	// Create cache object
	Cache c;

	// Loop 30 times
	for (int i = 0; i < 30; i++)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
		c.Add(i, f);
	}

	CHECK_EQUAL(30, c.Count()); // Cache should still have all 30 items, because the current frame is 0
	CHECK_EQUAL(20, c.GetMaxFrames()); // Max frames should default to 20
}

TEST(Cache_Max_Frames_Constructor)
{
	// Create cache object (with a max of 5 previous items)
	Cache c(5);

	// Loop 20 times
	for (int i = 20; i > 0; i--)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
		c.Add(i, f);
	}

	// Cache should have all 20 (since current frame is 0)
	CHECK_EQUAL(20, c.Count());

	// Set current frame to 15
	c.SetCurrentFrame(15);

	// Since current frame is 15, only 5 previous items are allowed (i.e. frame 10 to 20)
	CHECK_EQUAL(11, c.Count());

	// Add 10 frames again
	for (int i = 9; i > 0; i--)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
		c.Add(i, f);
	}

	// Count should be 20, since adding frames does not clean up old ones
	CHECK_EQUAL(20, c.Count());

	// Set current frame to 15, which should clean up many older frames
	c.SetCurrentFrame(15);

	// Count should be 11, since we've cleaned up again
	CHECK_EQUAL(11, c.Count());

	// Check which 10 items the cache kept
	CHECK_EQUAL(false, c.Exists(1));
	CHECK_EQUAL(false, c.Exists(5));
	CHECK_EQUAL(false, c.Exists(9));
	CHECK_EQUAL(true, c.Exists(10));
	CHECK_EQUAL(true, c.Exists(11));
	CHECK_EQUAL(true, c.Exists(15));
	CHECK_EQUAL(true, c.Exists(19));
	CHECK_EQUAL(true, c.Exists(20));

	// Since we're not adding any new frames, the count should decrease by 1
	c.SetCurrentFrame(16);
	CHECK_EQUAL(10, c.Count());

	// Since we're not adding any new frames, the count should decrease by 1
	c.SetCurrentFrame(17);
	CHECK_EQUAL(9, c.Count());

	// Since we're not adding any new frames, the count should decrease by 1
	c.SetCurrentFrame(18);
	CHECK_EQUAL(8, c.Count());

	// Increase frames to 50
	for (int i = 1; i <= 50; i++)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
		c.Add(i, f);
	}

	// Set current frame, which cleans up old frames
	c.SetCurrentFrame(18);

	// Count should be 38 (5 previous to 18 + all frames after 18)
	CHECK_EQUAL(38, c.Count());

}

TEST(Cache_Clear)
{
	// Create cache object (with a max of 10 previous items)
	Cache c(10);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
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
	// Create cache object (with a max of 10 items)
	Cache c(10);

	// Loop 10 times
	for (int i = 0; i < 10; i++)
	{
		// Add blank frame to the cache (each frame is #1)
		Frame *f = new Frame();
		c.Add(1, f);
	}

	// Cache should only have 1 items (since all frames were frame #1)
	CHECK_EQUAL(1, c.Count());
}

TEST(Cache_Check_If_Frame_Exists)
{
	// Create cache object (with a max of 5 items)
	Cache c(5);

	// Loop 5 times
	for (int i = 1; i < 6; i++)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
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
	// Create cache object (with a max of 10 items)
	Cache c(10);

	// Create 3 frames
	Frame red(1, 300, 300, "red");
	Frame blue(2, 400, 400, "blue");
	Frame green(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red.number, &red);
	c.Add(blue.number, &blue);
	c.Add(green.number, &green);

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
	Cache c(10);

	// Create 3 frames
	Frame blue(2, 400, 400, "blue");
	Frame red(1, 300, 300, "red");
	Frame green(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red.number, &red);
	c.Add(blue.number, &blue);
	c.Add(green.number, &green);

	// Check if frame 1 is the front
	CHECK_EQUAL(1, c.GetSmallestFrame()->number);

	// Check if frame 1 is STILL the front
	CHECK_EQUAL(1, c.GetSmallestFrame()->number);

	// Erase frame 1
	c.Remove(1, false);

	// Check if frame 2 is the front
	CHECK_EQUAL(2, c.GetSmallestFrame()->number);
}

TEST(Cache_Remove)
{
	// Create cache object (with a max of 10 items)
	Cache c(10);

	// Create 3 frames
	Frame red(1, 300, 300, "red");
	Frame blue(2, 400, 400, "blue");
	Frame green(3, 500, 500, "green");

	// Add frames to cache
	c.Add(red.number, &red);
	c.Add(blue.number, &blue);
	c.Add(green.number, &green);

	// Check if count is 3
	CHECK_EQUAL(3, c.Count());

	// Check if frame 2 exists
	CHECK_EQUAL(true, c.Exists(2));

	// Remove frame 2
	c.Remove(2, false);

	// Check if frame 2 exists
	CHECK_EQUAL(false, c.Exists(2));

	// Check if count is 2
	CHECK_EQUAL(2, c.Count());

	// Remove frame 1
	c.Remove(1, false);

	// Check if frame 1 exists
	CHECK_EQUAL(false, c.Exists(1));

	// Check if count is 1
	CHECK_EQUAL(1, c.Count());
}

TEST(Cache_Current_Frame)
{
	// Create cache object
	Cache c;

	// Loop 20 times
	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
		c.Add(i, f);
	}

	CHECK_EQUAL(0, c.GetCurrentFrame()); // Cache defaults current frame is 0

	// Set current frame
	c.SetCurrentFrame(10);

	CHECK_EQUAL(10, c.GetCurrentFrame()); // Cache should now be on frame 10

	// Set current frame
	c.SetCurrentFrame(20);

	CHECK_EQUAL(20, c.GetCurrentFrame()); // Cache should now be on frame 20
}

TEST(Cache_Set_Max_Frames)
{
	// Create cache object
	Cache c;

	// Loop 20 times
	for (int i = 0; i < 20; i++)
	{
		// Add blank frame to the cache
		Frame *f = new Frame();
		c.Add(i, f);
	}

	CHECK_EQUAL(20, c.GetMaxFrames()); // Cache defaults max frames to 20

	// Set max frames
	c.SetMaxFrames(10);
	CHECK_EQUAL(10, c.GetMaxFrames());

	// Set max frames
	c.SetMaxFrames(30);
	CHECK_EQUAL(30, c.GetMaxFrames());
}

