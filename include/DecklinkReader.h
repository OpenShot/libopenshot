#ifndef OPENSHOT_DECKLINK_READER_H
#define OPENSHOT_DECKLINK_READER_H

/**
 * \file
 * \brief Header file for ImageReader class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "FileReaderBase.h"

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

// Only found if the decklink SDK is installed (and a blackmagic card
// is installed on your computer)
#include "DeckLinkAPI.h"


using namespace std;


// Class definition used by Decklink API
class DeckLinkCaptureDelegate: public IDeckLinkInputCallback {
public:
	DeckLinkCaptureDelegate();
	~DeckLinkCaptureDelegate();

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) {
		return E_NOINTERFACE;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef(void);
	virtual ULONG STDMETHODCALLTYPE Release(void);
	virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(
			BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*,
			BMDDetectedVideoInputFormatFlags);
	virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(
			IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

private:
	ULONG m_refCount;
	pthread_mutex_t m_mutex;
};


namespace openshot
{

	/**
	 * \brief This class uses the Blackmagic Decklink libraries, to open video streams on Blackmagic devices, and return
	 * openshot::Frame objects containing the image and audio data.
	 */
	class DecklinkReader : public FileReaderBase
	{
	private:
		string path;
		tr1::shared_ptr<Magick::Image> image;
		bool is_open;

	public:

		/// Constructor for DecklinkReader.  This automatically opens the device and loads
		/// the first second of video, or it throws one of the following exceptions.
		DecklinkReader(string path) throw(InvalidFile);

		/// Close the device and video stream
		void Close();

		/// Get an openshot::Frame object for a specific frame number of this reader.  Frame number
		/// is ignored, since it always gets the latest LIVE frame.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed);

		/// Open device and video stream - which is called by the constructor automatically
		void Open() throw(InvalidFile);
	};

}

#endif
