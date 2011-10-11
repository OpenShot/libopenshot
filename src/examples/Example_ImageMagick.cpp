#include <iostream>
#include "Magick++.h"

using namespace std;
using namespace Magick;

int main()
{
	// Create base image (white image of 300 by 200 pixels)
	Image image( Geometry(300,200), Color("red") );
	image.matte(true);
	image.display();

	return 0;
}
