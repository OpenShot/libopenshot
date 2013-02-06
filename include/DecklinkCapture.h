#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <omp.h>

#include "DeckLinkAPI.h"
#include "../include/DecklinkCapture.h"
#include "../include/Frame.h"


class DeckLinkCaptureDelegate : public IDeckLinkInputCallback
{
public:
	pthread_cond_t*			sleepCond;
	BMDTimecodeFormat		g_timecodeFormat;
	unsigned long 			frameCount;

	// Queue of raw video frames
	deque<IDeckLinkMutableVideoFrame*> raw_video_frames;
	deque<tr1::shared_ptr<openshot::Frame> > final_frames;

	// Convert between YUV and RGB
	IDeckLinkOutput *deckLinkOutput;
	IDeckLinkVideoConversion *deckLinkConverter;

	DeckLinkCaptureDelegate(pthread_cond_t* m_sleepCond, IDeckLinkOutput* deckLinkOutput, IDeckLinkVideoConversion* deckLinkConverter);
	~DeckLinkCaptureDelegate();

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) { return E_NOINTERFACE; }
	virtual ULONG STDMETHODCALLTYPE AddRef(void);
	virtual ULONG STDMETHODCALLTYPE  Release(void);
	virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags);
	virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

	// Extra methods
	tr1::shared_ptr<openshot::Frame> GetFrame(int requested_frame);

private:
	ULONG				m_refCount;
	pthread_mutex_t		m_mutex;
};

#endif
