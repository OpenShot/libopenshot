/**
 * @file
 * @brief Header file for DecklinkWriter class
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

#ifndef OPENSHOT_DECKLINK_WRITER_H
#define OPENSHOT_DECKLINK_WRITER_H

#include "WriterBase.h"

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
#include "DecklinkOutput.h"

using namespace std;

namespace openshot
{

	/**
	 * @brief This class uses the Blackmagic Decklink libraries, to send video streams to Blackmagic devices.
	 *
	 * This requires special hardware manufactured by <a href="http://www.blackmagicdesign.com/products">Blackmagic Designs</a>.
	 * Once the device is aquired and connected, this reader returns openshot::Frame objects containing the image and audio data.
	 */
	class DecklinkWriter : public WriterBase
	{
	private:
		bool is_open;

		IDeckLink 					*deckLink;
		IDeckLinkDisplayModeIterator	*displayModeIterator;
		IDeckLinkOutput				*deckLinkOutput;
		IDeckLinkVideoConversion 	*m_deckLinkConverter;
		pthread_mutex_t				sleepMutex;
		pthread_cond_t				sleepCond;
		IDeckLinkIterator			*deckLinkIterator;
		DeckLinkOutputDelegate 		*delegate;
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

	public:

		/// Constructor for DecklinkWriter.  This automatically opens the device or it
		/// throws one of the following exceptions.
		DecklinkWriter(int device, int video_mode, int pixel_format, int channels, int sample_depth) throw(DecklinkError);

		/// Close the device and video stream
		void Close();

		/// This method is required for all derived classes of WriterBase.  Write a Frame to the video file.
		void WriteFrame(std::shared_ptr<Frame> frame) throw(WriterClosed);

		/// This method is required for all derived classes of WriterBase.  Write a block of frames from a reader.
		void WriteFrame(ReaderBase* reader, int start, int length) throw(WriterClosed);

		/// Open device and video stream - which is called by the constructor automatically
		void Open() throw(DecklinkError);

		/// Determine if writer is open or closed
		bool IsOpen() { return is_open; };


	};

}

#endif
