/**
 * @file
 * @brief Source file for SDLPlayer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/SDLPlayer.h"

using namespace openshot;

// Default constructor
SDLPlayer::SDLPlayer() {

};

// Display a loading animation
void SDLPlayer::Loading() {
	mode = PLAYBACK_LOADING;
}

// Pause the video
void SDLPlayer::Pause() {
	mode = PLAYBACK_PAUSED;
}

// Stop the video player and clear the cached frames
void SDLPlayer::Stop() {
	mode = PLAYBACK_STOPPED;
}

/// Get the current frame number being played
int SDLPlayer::Position() {
	return position;
}

/// Seek to a specific frame in the player
void SDLPlayer::Seek(int new_frame) {
	position = new_frame;
}

// Play the video
void SDLPlayer::Play()
{
	// Set mode
	mode = PLAYBACK_PLAY;

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
	SDL_Overlay *bmp;
	bmp = SDL_CreateYUVOverlay(reader->info.width, reader->info.height, SDL_YV12_OVERLAY, screen);

	cout << setprecision(6);
	cout << "START PREPARING SURFACES..." << endl;

	for (int stuff = 0; stuff < number_of_cycles; stuff++)
	{
		// Get pointer to pixels of image.
		tr1::shared_ptr<openshot::Frame> f = reader->GetFrame(300 + stuff);

		// Create YUV Overlay
		SDL_Overlay *bmp;
		bmp = SDL_CreateYUVOverlay(reader->info.width, reader->info.height, SDL_YV12_OVERLAY, screen);
		SDL_LockYUVOverlay(bmp);

		// Get luminance of each pixel
		int pixel_index = 0;
		int biggest_y = 0;
		int smallest_y = 512;
		for (int row = 0; row < screen->h; row++) {
			// Get array of pixels for this row
			//cout << "row: " << row << endl;
			const Magick::PixelPacket *imagepixels = f->GetPixels(row);

			// Loop through pixels on this row
			for (int column = 0; column < screen->w; column++) {

				// Get a pixel from this row
				const Magick::PixelPacket *pixel = imagepixels;

				// Get the RGB colors
				float r = pixel[column].red / 255.0;
				float b = pixel[column].blue / 255.0;
				float g = pixel[column].green / 255.0;

				// Calculate the Y value (brightness or luminance)
				float y = (0.299 * r) + (0.587 * g) + (0.114 * b);

				// Update the Y value for every pixel
				bmp->pixels[0][pixel_index] = y;

				// Increment counter
				pixel_index++;

			}
		}

		// Get pixels for resized frame (for reduced color needed by YUV420p)
		int divider = 2;
		tr1::shared_ptr<Magick::Image> reduced_color = tr1::shared_ptr<Magick::Image>(new Magick::Image(*f->GetImage().get())); // copy frame image
		Magick::Geometry new_size;
		new_size.width(reader->info.width / divider);
		new_size.height(reader->info.height / divider);
		reduced_color->scale(new_size);

		int number_of_colors = (reader->info.width / divider) * (reader->info.height / divider);

		// Loop through the UV (color info)
		for (int pixel_index = 0; pixel_index < number_of_colors; pixel_index++)
		{
			// Get a pixel from this row
			const Magick::PixelPacket *pixel = reduced_color->getConstPixels(0,0, new_size.width(), new_size.height());

			// Get the RGB colors
			float r = pixel[pixel_index].red / 255.0;
			float b = pixel[pixel_index].blue / 255.0;
			float g = pixel[pixel_index].green / 255.0;

			// Calculate UV colors
			float v = (0.439 * r) - (0.368 * g) - (0.071 * b) + 128;
			float u = (-0.148 * r) - (0.291 * g) + (0.439 * b) + 128;

			// Update the UV values for every pixel
			bmp->pixels[1][pixel_index] = v * 1.0;
			bmp->pixels[2][pixel_index] = u * 1.0;

		}
		SDL_UnlockYUVOverlay(bmp);

		// Add to vector
		overlays.push_back(bmp);
	}

	cout << "START DISPLAYING SURFACES..." << endl;

	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = reader->info.width;
	rect.h = reader->info.height;

	// Loop through overlays and display them
	for (int repeat = 0; repeat < 3; repeat++)
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

	cout << "DONE!" << endl;
	SDL_Delay(1000);
}

