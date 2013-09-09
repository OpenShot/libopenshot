#ifndef OPENSHOT_DECKLINK_READER_H
#define OPENSHOT_DECKLINK_READER_H

/**
 * \file
 * \brief Header file for ImageReader class
 * \author Copyright (c) 2008-2013 OpenShot Studios, LLC
 */

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
#include <tr1/memory>
#include <unistd.h>

#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"
#include "Frame.h"
#include "DecklinkInput.h"

using namespace std;

namespace openshot
{

	/**
	 * \brief This class uses the Blackmagic Decklink libraries, to open video streams on Blackmagic devices, and return
	 * openshot::Frame objects containing the image and audio data.
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

		/// Get an openshot::Frame object for a specific frame number of this reader.  Frame number
		/// is ignored, since it always gets the latest LIVE frame.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed);
		unsigned long GetCurrentFrameNumber();

		/// Open device and video stream - which is called by the constructor automatically
		void Open() throw(DecklinkError);
	};

}

#endif
