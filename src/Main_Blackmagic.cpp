
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <tr1/memory>
#include "../include/OpenShot.h"
#include <omp.h>

using namespace openshot;


int main(int argc, char *argv[])
{
	IDeckLink 						*deckLink;
	IDeckLinkInput					*deckLinkInput;
	IDeckLinkDisplayModeIterator	*displayModeIterator;
	IDeckLinkOutput					*m_deckLinkOutput;
	IDeckLinkVideoConversion 		*m_deckLinkConverter;

	pthread_mutex_t				sleepMutex;
	pthread_cond_t				sleepCond;
	IDeckLinkIterator			*deckLinkIterator = CreateDeckLinkIteratorInstance();
	DeckLinkCaptureDelegate 	*delegate;
	IDeckLinkDisplayMode		*displayMode;
	BMDVideoInputFlags			inputFlags = 0;
	BMDDisplayMode				selectedDisplayMode = bmdModeNTSC;
	BMDPixelFormat				pixelFormat = bmdFormat8BitYUV;
	int							displayModeCount = 0;
	int							exitStatus = 1;
	int							ch;
	bool 						foundDisplayMode = false;
	HRESULT						result;

	int						g_videoModeIndex = -1;
	int						g_audioChannels = 2;
	int						g_audioSampleDepth = 16;
	int						g_maxFrames = 50;

	pthread_mutex_init(&sleepMutex, NULL);
	pthread_cond_init(&sleepCond, NULL);

	if (!deckLinkIterator)
	{
		fprintf(stderr, "This application requires the DeckLink drivers installed.\n");
		goto bail;
	}

	/* Connect to the first DeckLink instance */
	result = deckLinkIterator->Next(&deckLink);
	if (result != S_OK)
	{
		fprintf(stderr, "No DeckLink PCI cards found.\n");
		goto bail;
	}

	if (deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&deckLinkInput) != S_OK)
		goto bail;

	// Obtain an IDeckLinkDisplayModeIterator to enumerate the display modes supported on output
	result = deckLinkInput->GetDisplayModeIterator(&displayModeIterator);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the video output display mode iterator - result = %08x\n", result);
		goto bail;
	}

	// Init deckLinkOutput (needed for color conversion)
	if (deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&m_deckLinkOutput) != S_OK)
	{
		cout << "Failed to create a deckLinkOutput(), used to convert YUV to RGB." << endl;
		m_deckLinkOutput = NULL;
	}

	// Init the YUV to RGB conversion
	if(!(m_deckLinkConverter = CreateVideoConversionInstance()))
	{
		cout << "Failed to create a VideoConversionInstance(), used to convert YUV to RGB." << endl;
		m_deckLinkConverter = NULL;
	}


	// Create Delegate & Pass in pointers to the output and converters
	delegate = new DeckLinkCaptureDelegate(&sleepCond, m_deckLinkOutput, m_deckLinkConverter);
	deckLinkInput->SetCallback(delegate);


	// Parse command line options
	while ((ch = getopt(argc, argv, "?h3c:s:f:a:m:n:p:t:")) != -1)
	{
		switch (ch)
		{
			case 'm':
				g_videoModeIndex = atoi(optarg);
				break;
			case 'c':
				g_audioChannels = atoi(optarg);
				if (g_audioChannels != 2 &&
				    g_audioChannels != 8 &&
					g_audioChannels != 16)
				{
					fprintf(stderr, "Invalid argument: Audio Channels must be either 2, 8 or 16\n");
					goto bail;
				}
				break;
			case 's':
				g_audioSampleDepth = atoi(optarg);
				if (g_audioSampleDepth != 16 && g_audioSampleDepth != 32)
				{
					fprintf(stderr, "Invalid argument: Audio Sample Depth must be either 16 bits or 32 bits\n");
					goto bail;
				}
				break;
			case 'n':
				g_maxFrames = atoi(optarg);
				break;
			case '3':
				inputFlags |= bmdVideoInputDualStream3D;
				break;
			case 'p':
				switch(atoi(optarg))
				{
					case 0: pixelFormat = bmdFormat8BitYUV; break;
					case 1: pixelFormat = bmdFormat10BitYUV; break;
					case 2: pixelFormat = bmdFormat10BitRGB; break;
					default:
						fprintf(stderr, "Invalid argument: Pixel format %d is not valid", atoi(optarg));
						goto bail;
				}
				break;
			case 't':
				{
					fprintf(stderr, "Invalid argument: Timecode format \"%s\" is invalid\n", optarg);
					goto bail;
				}
				break;
			//case '?':
			//case 'h':
			//	usage(0);
		}
	}

	if (g_videoModeIndex < 0)
	{
		fprintf(stderr, "No video mode specified\n");
		//usage(0);
	}

	while (displayModeIterator->Next(&displayMode) == S_OK)
	{
		if (g_videoModeIndex == displayModeCount)
		{
			BMDDisplayModeSupport result;
			const char *displayModeName;

			foundDisplayMode = true;
			displayMode->GetName(&displayModeName);
			selectedDisplayMode = displayMode->GetDisplayMode();

			deckLinkInput->DoesSupportVideoMode(selectedDisplayMode, pixelFormat, bmdVideoInputFlagDefault, &result, NULL);

			if (result == bmdDisplayModeNotSupported)
			{
				fprintf(stderr, "The display mode %s is not supported with the selected pixel format\n", displayModeName);
				goto bail;
			}

			if (inputFlags & bmdVideoInputDualStream3D)
			{
				if (!(displayMode->GetFlags() & bmdDisplayModeSupports3D))
				{
					fprintf(stderr, "The display mode %s is not supported with 3D\n", displayModeName);
					goto bail;
				}
			}

			break;
		}
		displayModeCount++;
		displayMode->Release();
	}

	if (!foundDisplayMode)
	{
		fprintf(stderr, "Invalid mode %d specified\n", g_videoModeIndex);
		goto bail;
	}

    result = deckLinkInput->EnableVideoInput(selectedDisplayMode, pixelFormat, inputFlags);
    if(result != S_OK)
    {
		fprintf(stderr, "Failed to enable video input. Is another application using the card?\n");
        goto bail;
    }

    result = deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, g_audioSampleDepth, g_audioChannels);
    if(result != S_OK)
    {
        goto bail;
    }

	result = deckLinkInput->StartStreams();
    if(result != S_OK)
    {
        goto bail;
    }

    // Test GetFrame() method
    for (int x = 0; x < 10000; x++)
    {
    	tr1::shared_ptr<openshot::Frame> f = delegate->GetFrame(0);
    	//if (f)
    	//	cout << "Found Frame!" << endl;
    	//else
    	//	cout << "No Frame Found" << endl;

   		usleep(20000);
    }

	// All Okay.
	exitStatus = 0;

	// Block main thread until signal occurs
	pthread_mutex_lock(&sleepMutex);
	pthread_cond_wait(&sleepCond, &sleepMutex);
	pthread_mutex_unlock(&sleepMutex);
	fprintf(stderr, "Stopping Capture\n");

bail:

	if (displayModeIterator != NULL)
	{
		displayModeIterator->Release();
		displayModeIterator = NULL;
	}

    if (deckLinkInput != NULL)
    {
        deckLinkInput->Release();
        deckLinkInput = NULL;
    }

    if (deckLink != NULL)
    {
        deckLink->Release();
        deckLink = NULL;
    }

	if (deckLinkIterator != NULL)
		deckLinkIterator->Release();

    return exitStatus;
}
