#ifndef OPENSHOT_DECKLINK_WRITER_H
#define OPENSHOT_DECKLINK_WRITER_H

/**
 * @file
 * @brief Header file for ImageReader class
 * @author Copyright (c) 2008-2013 OpenShot Studios, LLC
 */

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
#include <tr1/memory>
#include <unistd.h>

#include "Magick++.h"
#include "Cache.h"
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
		void WriteFrame(tr1::shared_ptr<Frame> frame);

		/// This method is required for all derived classes of WriterBase.  Write a block of frames from a reader.
		void WriteFrame(ReaderBase* reader, int start, int length);

		/// Open device and video stream - which is called by the constructor automatically
		void Open() throw(DecklinkError);
	};

}

#endif
