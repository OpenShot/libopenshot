/**
 * @file
 * @brief Header file for DecklinkInput class
 * @author Jonathan Thomas <jonathan@openshot.org>, Blackmagic Design
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
// Copyright (c) 2009 Blackmagic Design
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_DECKLINK_INPUT_H
#define OPENSHOT_DECKLINK_INPUT_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include "DeckLinkAPI.h"
#include "Frame.h"
#include "CacheMemory.h"
#include "OpenMPUtilities.h"

/// Implementation of the Blackmagic Decklink API (used by the DecklinkReader)
class DeckLinkInputDelegate : public IDeckLinkInputCallback
{
public:
	pthread_cond_t*			sleepCond;
	BMDTimecodeFormat		g_timecodeFormat;
	unsigned long 			frameCount;
	unsigned long 			final_frameCount;

	// Queue of raw video frames
	std::deque<IDeckLinkMutableVideoFrame*> raw_video_frames;
	openshot::CacheMemory final_frames;

	// Convert between YUV and RGB
	IDeckLinkOutput *deckLinkOutput;
	IDeckLinkVideoConversion *deckLinkConverter;

	DeckLinkInputDelegate(pthread_cond_t* m_sleepCond, IDeckLinkOutput* deckLinkOutput, IDeckLinkVideoConversion* deckLinkConverter);
	~DeckLinkInputDelegate();

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) { return E_NOINTERFACE; }
	virtual ULONG STDMETHODCALLTYPE AddRef(void);
	virtual ULONG STDMETHODCALLTYPE  Release(void);
	virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags);
	virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

	// Extra methods
	std::shared_ptr<openshot::Frame> GetFrame(int64_t requested_frame);
	unsigned long GetCurrentFrameNumber();

private:
	ULONG				m_refCount;
	pthread_mutex_t		m_mutex;
};

#endif
