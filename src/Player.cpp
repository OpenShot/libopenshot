#include "../include/Player.h"

using namespace openshot;

// Default constructor
Player::Player() {

};

// Set the current reader, such as a FFmpegReader
void Player::SetReader(FileReaderBase *p_reader)
{
	// Set the reader
	reader = p_reader;
};

// Set a callback function which will be invoked each time a frame is ready to be displayed
void Player::SetFrameCallback(CallbackPtr p_callback, void *p_pythonmethod)
{
	// set the private function pointer
	callback = p_callback;
	pythonmethod = p_pythonmethod;


//	double *Pixels = new double[100];
//	for (int i = 0; i < 100; i++)
//		Pixels[i] = i + 100;
	tr1::shared_ptr<Frame> f = reader->GetFrame(300);

	// invoke method pointer 10 times
	for (int i = 0; i < 30; i++)
		callback(i, f->GetWidth(), f->GetHeight(), f->GetPixels(), pythonmethod);
};

// Manually invoke function (if any)
void Player::Push()
{
	cout << "callback: " << &callback << endl;
	cout << "pythonmethod: " << &pythonmethod << endl;

//	double *Pixels = new double[100];
//	for (int i = 0; i < 100; i++)
//		Pixels[i] = i + 200;
	tr1::shared_ptr<Frame> f = reader->GetFrame(500);

	// manually invoke method
	for (int i = 30; i < 20; i++)
		callback(i, f->GetWidth(), f->GetHeight(), f->GetPixels(), pythonmethod);
}

// Play the video
void Player::Play()
{
	// Init SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) {
		fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
		exit(1);
	}

	// Create an SDL surface
	SDL_Surface *screen;
	screen = SDL_SetVideoMode(reader->info.width, reader->info.height, 0, SDL_HWSURFACE | SDL_ANYFORMAT | SDL_DOUBLEBUF);
	if (!screen) {
		fprintf(stderr, "SDL: could not set video mode - exiting\n");
		exit(1);
	}

	vector<SDL_Overlay*> overlays;
	int number_of_cycles = 60;


// Create YUV Overlay
//SDL_Overlay *bmp;
//bmp = SDL_CreateYUVOverlay(reader->info.width, reader->info.height, SDL_YV12_OVERLAY, screen);

cout << setprecision(6);
cout << "START PREPARING SURFACES..." << endl;

//for (int stuff = 0; stuff < number_of_cycles; stuff++)
//{
//	// Get pointer to pixels of image.
//	Frame *f = reader->GetFrame(300 + stuff);
//
//	// Create YUV Overlay
//	SDL_Overlay *bmp;
//	bmp = SDL_CreateYUVOverlay(reader->info.width, reader->info.height, SDL_YV12_OVERLAY, screen);
//	SDL_LockYUVOverlay(bmp);
//
//	// Get pixels for resized frame (for reduced color needed by YUV420p)
//	int divider = 2;
//	//const Magick::PixelPacket *reduced_color = f->GetPixels(reader->info.width / divider, reader->info.height / divider, f->number);
//	int number_of_colors = (reader->info.width / divider) * (reader->info.height / divider);
//
//	int pixel_index = 0;
//	int biggest_y = 0;
//	int smallest_y = 512;
//	for (int row = 0; row < screen->h; row++) {
//		// Get array of pixels for this row
//		//cout << "row: " << row << endl;
//		const Magick::PixelPacket *imagepixels = f->GetPixels(row);
//
//		// Loop through pixels on this row
//		for (int column = 0; column < screen->w; column++) {
//
//			// Get a pixel from this row
//			const Magick::PixelPacket *pixel = imagepixels;
//
//			// Get the RGB colors
//			float r = pixel[column].red / 255.0;
//			float b = pixel[column].blue / 255.0;
//			float g = pixel[column].green / 255.0;
//
//			// Calculate the Y value (brightness or luminance)
//			float y = (0.299 * r) + (0.587 * g) + (0.114 * b);
//
////			if (y > biggest_y)
////				biggest_y = y;
////			if (y < smallest_y)
////				smallest_y = y;
//
//			// Update the Y value for every pixel
//			bmp->pixels[0][pixel_index] = y;
//			//bmp->pixels[1][pixel_index] = 0;
//			//bmp->pixels[2][pixel_index] = 0;
//
//			// Increment counter
//			pixel_index++;
//
//		}
//	}
//
////	cout << "Biggest Y: " << biggest_y << ", Smallest Y: " << smallest_y << endl;
////	cout << "ADD COLOR TO YUV OVERLAY" << endl;
//
//	// Loop through the UV (color info)
//	//int color_counter = 511;
//	//number_of_colors = bmp->pitches[1] * 218;
////	int biggest_v = 0;
////	int smallest_v = 512;
////	int biggest_u = 0;
////	int smallest_u = 512;
//	for (int pixel_index = 0; pixel_index < number_of_colors; pixel_index++)
//	{
//		// Get a pixel from this row
//		const Magick::PixelPacket *pixel = reduced_color;
//
//		// Get the RGB colors
//		float r = pixel[pixel_index].red / 255.0;
//		float b = pixel[pixel_index].blue / 255.0;
//		float g = pixel[pixel_index].green / 255.0;
////		float r = 100.0;
////		float g = 100.0;
////		float b = 100.0;
//
//		// Calculate UV colors
//		float v = (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
//		float u = (-0.148 * r) - (0.291 * g) + (0.439 * b) + 128;
//
////		// Grey pixel
////		if (pixel_index == 40650)
////		{
////			cout << "GREY FOUND!!!" << endl;
////			cout << "r: " << int(r) << ", g: " << int(g) << ", b: " << int(b) << "    v: " << int(v) << ", u: " << int(u) << endl;
////		}
////
////		// Pink pixel
////		if (pixel_index == 42698)
////		{
////			cout << "PINK FOUND!!!" << endl;
////			cout << "r: " << int(r) << ", g: " << int(g) << ", b: " << int(b) << "    v: " << int(v) << ", u: " << int(u) << endl;
////		}
//
////		if (v > 255.0 || v <= 0.0)
////			cout << "TOO BIG v!!!!" << endl;
////		if (u > 255.0 || u <= 0.0)
////			cout << "TOO BIG u!!!!" << endl;
//
////		if (v > biggest_v)
////			biggest_v = v;
////		if (v < smallest_v)
////			smallest_v = v;
////
////		if (u > biggest_u)
////			biggest_u = u;
////		if (u < smallest_u)
////			smallest_u = u;
//
//		// Update the UV values for every pixel
//		bmp->pixels[1][pixel_index] = v * 1.0;
//		bmp->pixels[2][pixel_index] = u * 1.0;
//
//		//color_counter++;
//	}
//
//	//cout << "Biggest V: " << biggest_v << ", Smallest V: " << smallest_v << endl;
//	//cout << "Biggest U: " << biggest_u << ", Smallest U: " << smallest_u << endl;
//
//	SDL_UnlockYUVOverlay(bmp);
//
//	// Add to vector
//	overlays.push_back(bmp);
//
//	// Update surface.
//	//SDL_UpdateRect(screen, 0, 0, reader->info.width, reader->info.height);
//}

cout << "START DISPLAYING SURFACES..." << endl;

SDL_Rect rect;
rect.x = 0;
rect.y = 0;
rect.w = reader->info.width;
rect.h = reader->info.height;

//cout << "PRESS A BUTTON TO PLAY THE VIDEO" << endl;
//string name = "";
//cin >> name;

for (int repeat = 0; repeat < 10; repeat++)
{
	cout << "START OVERLAY LOOP:" << endl;
	for (int z = 0; z < number_of_cycles; z++)
	{
		cout << z << endl;
		//SDL_LockSurface( screen);
		//primary_screen->pixels = surfaces[z].pixels;
		//SDL_UnlockSurface(primary_screen);
		//SDL_UpdateRect(screen, 0, 0, reader->info.width, reader->info.height);

		SDL_DisplayYUVOverlay(overlays[z], &rect);
		//SDL_UnlockSurface( screen);

		SDL_Delay(41);
	}
}

// Works!
//cout << "SDL_RECT" << endl;
//SDL_Rect rect;
//rect.x = 0;
//rect.y = 0;
//rect.w = reader->info.width;
//rect.h = reader->info.height;
//SDL_DisplayYUVOverlay(bmp, &rect);


cout << "DONE!" << endl;

	SDL_Delay(3000);
}

