
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

	DecklinkReader dr(1, 11, 0, 2, 16);
	dr.Open();

	DecklinkWriter w(0, 11, 3, 2, 16);
	w.Open();

	// Loop through reader
	while (true)
	{
		tr1::shared_ptr<Frame> f = dr.GetFrame(0);
		if (f)
		{
			//f->Display();
			w.WriteFrame(f);
			//usleep(1000 * 1);
		}
	}

	// Sleep
	sleep(4);

	// Close writer
	w.Close();

	return 0;
}
