#ifndef OPENSHOT_PLAYER_H
#define OPENSHOT_PLAYER_H

/**
 * \file
 * \brief Header file for Frame class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include <iostream>
#include <vector>
#include "../include/FileReaderBase.h"
#include <SDL.h>
#include <SDL_thread.h>

using namespace std;

namespace openshot
{
	typedef void (*CallbackPtr)(int, int, int, const Magick::PixelPacket *Pixels, void *);

	/**
	 * \brief This class is used to playback a video from a reader.
	 *
	 * This player does not actually show the video, but rather it invokes a method each time
	 * a frame should be displayed.  This allows the calling application to display the image using
	 * any toolkit it wishes.
	 */
	class Player
	{
	private:
		FileReaderBase *reader;
		CallbackPtr callback;
		void *pythonmethod;

		// draw pixels for SDL
		void putpixel(SDL_Surface *surface, SDL_Overlay *bmp, int x, int y, Uint32 pixel, int pixel_index);

	public:
		/// Default constructor
		Player();

		/// Set the current reader, such as a FFmpegReader
		void SetReader(FileReaderBase *p_reader);

		/// Set a callback function which will be invoked each time a frame is ready to be displayed
		void SetFrameCallback(CallbackPtr p_callback, void *p_pythonmethod);

		/// Manually invoke function (if any)
		void Push();

		/// Play the video
		void Play();

	};

}

#endif
