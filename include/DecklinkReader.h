/**
 * @file
 * @brief Header file for DecklinkReader class
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

#ifndef OPENSHOT_DECKLINK_READER_H
#define OPENSHOT_DECKLINK_READER_H

#include "ReaderBase.h"

#include <cmath>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <omp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <unistd.h>

#include "CacheMemory.h"
#include "Exceptions.h"
#include "Frame.h"
#include "DecklinkInput.h"

using namespace std;

namespace openshot
{

	/**
	 * @brief This class uses the Blackmagic Decklink libraries, to open video streams on Blackmagic devices.
	 *
	 * This requires special hardware manufactured by <a href="http://www.blackmagicdesign.com/products">Blackmagic Designs</a>.
	 * Once the device is aquired and connected, this reader returns openshot::Frame objects containing the image and audio data.
	 */
	class DecklinkReader : public ReaderBase
	{
	private:
		bool is_open;

		IDeckLink 					*deckLink;
		IDeckLinkInput				*deckLinkInput;
		IDeckLinkDisplayModeIterator	*displayModeIterator;
		IDeckLinkOutput				*m_deckLinkOutput;
		IDeckLinkVideoConversion 	*m_deckLinkConverter;
		pthread_mutex_t				sleepMutex;
		pthread_cond_t				sleepCond;
		IDeckLinkIterator			*deckLinkIterator;
		DeckLinkInputDelegate 		*delegate;
		IDeckLinkDisplayMode		*displayMode;
		BMDVideoInputFlags			inputFlags;
		BMDDisplayMode				selectedDisplayMode;
		BMDPixelFormat				pixelFormat;
		int							displayModeCount;
		int							exitStatus;
		int							ch;
		bool 						foundDisplayMode;
		HRESULT						result;
		int							g_videoModeIndex;
		int							g_audioChannels;
		int							g_audioSampleDepth;
		int							g_maxFrames;
		int							device;
		BMDTimeValue frameRateDuration, frameRateScale;
		const char *displayModeName;

	public:

		/// Constructor for DecklinkReader.  This automatically opens the device and loads
		/// the first second of video, or it throws one of the following exceptions.
		DecklinkReader(int device, int video_mode, int pixel_format, int channels, int sample_depth) throw(DecklinkError);
		~DecklinkReader(); /// Destructor

		/// Close the device and video stream
		void Close();

		/// Get the cache object used by this reader (always returns NULL for this reader)
		CacheMemory* GetCache() { return NULL; };

		/// Get an openshot::Frame object for a specific frame number of this reader.  Frame number
		/// is ignored, since it always gets the latest LIVE frame.
		///
		/// @returns The requested frame (containing the image)
		/// @param requested_frame The frame number that is requested.
		std::shared_ptr<Frame> GetFrame(long int requested_frame) throw(ReaderClosed);
		unsigned long GetCurrentFrameNumber();

		/// Determine if reader is open or closed
		bool IsOpen() { return is_open; };

		/// Return the type name of the class
		string Name() { return "DecklinkReader"; };

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root) throw(InvalidFile); ///< Load Json::JsonValue into this object

		/// Open device and video stream - which is called by the constructor automatically
		void Open() throw(DecklinkError);
	};

}

#endif
