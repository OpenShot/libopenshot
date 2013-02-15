#ifndef OPENSHOT_DECKLINK_OUTPUT_H
#define OPENSHOT_DECKLINK_OUTPUT_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <omp.h>
#include "Magick++.h"

#include "DeckLinkAPI.h"
#include "../include/Cache.h"
#include "../include/Frame.h"

enum OutputSignal {
	kOutputSignalPip		= 0,
	kOutputSignalDrop		= 1
};

class DeckLinkOutputDelegate : public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback
{
protected:
	unsigned long					m_totalFramesScheduled;
	OutputSignal					m_outputSignal;
	void*							m_audioBuffer;
	unsigned long					m_audioBufferSampleLength;
	unsigned long					m_audioBufferOffset;
	unsigned long					m_audioChannelCount;
	BMDAudioSampleRate				m_audioSampleRate;
	unsigned long					m_audioSampleDepth;
	unsigned long					audioSamplesPerFrame;
	unsigned long					m_framesPerSecond;
	int								height;
	int								width;

	unsigned long 							frameCount;
	//map<int, IDeckLinkMutableVideoFrame* > 	temp_cache;
	map<int, uint8_t * > 	temp_cache;

	BMDTimeValue frameRateDuration, frameRateScale;

	// Queue of raw video frames
	//deque<IDeckLinkMutableVideoFrame*> final_frames;
	deque<uint8_t * > final_frames;
	deque<tr1::shared_ptr<openshot::Frame> > raw_video_frames;

	// Convert between YUV and RGB
	IDeckLinkOutput *deckLinkOutput;
	IDeckLinkDisplayMode *displayMode;

	// Current frame being displayed
	IDeckLinkMutableVideoFrame *m_currentFrame;

public:
	DeckLinkOutputDelegate(IDeckLinkDisplayMode *displayMode, IDeckLinkOutput* deckLinkOutput);
	~DeckLinkOutputDelegate();

	// *** DeckLink API implementation of IDeckLinkVideoOutputCallback IDeckLinkAudioOutputCallback *** //
	// IUnknown needs only a dummy implementation
	virtual HRESULT STDMETHODCALLTYPE	QueryInterface (REFIID iid, LPVOID *ppv)	{return E_NOINTERFACE;}
	virtual ULONG STDMETHODCALLTYPE		AddRef ()									{return 1;}
	virtual ULONG STDMETHODCALLTYPE		Release ()									{return 1;}

	virtual HRESULT STDMETHODCALLTYPE	ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
	virtual HRESULT STDMETHODCALLTYPE	ScheduledPlaybackHasStopped ();

	virtual HRESULT STDMETHODCALLTYPE	RenderAudioSamples (bool preroll);

	/// Schedule the next frame
	void ScheduleNextFrame(bool prerolling);

	/// Custom method to write new frames
	void WriteFrame(tr1::shared_ptr<openshot::Frame> frame);

private:
	ULONG				m_refCount;
	pthread_mutex_t		m_mutex;
};


#endif
