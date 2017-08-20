/**
 * @file
 * @brief Source file for DecklinkWriter class
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

#include "../include/DecklinkWriter.h"

using namespace openshot;

DecklinkWriter::DecklinkWriter(int device, int video_mode, int pixel_format, int channels, int sample_depth) throw(DecklinkError)
	: device(device), is_open(false), g_videoModeIndex(video_mode), g_audioChannels(channels), g_audioSampleDepth(sample_depth)
{
	// Init decklink variables
	inputFlags = 0;
	selectedDisplayMode = bmdModeNTSC;
	pixelFormat = bmdFormat8BitYUV;
	displayModeCount = 0;
	exitStatus = 1;
	foundDisplayMode = false;
	pthread_mutex_init(&sleepMutex, NULL);
	pthread_cond_init(&sleepCond, NULL);

	switch(pixel_format)
	{
		case 0: pixelFormat = bmdFormat8BitYUV; break;
		case 1: pixelFormat = bmdFormat10BitYUV; break;
		case 2: pixelFormat = bmdFormat10BitRGB; break;
		case 3: pixelFormat = bmdFormat8BitARGB; break;
		default:
			throw DecklinkError("Pixel format is not valid (must be 0,1,2,3).");
	}
}

// Open decklink writer
void DecklinkWriter::Open() throw(DecklinkError)
{
	// Open reader if not already open
	if (!is_open)
	{
		// Attempt to open blackmagic card
		deckLinkIterator = CreateDeckLinkIteratorInstance();

		if (!deckLinkIterator)
			throw DecklinkError("This application requires the DeckLink drivers installed.");

		/* Connect to a DeckLink instance */
		for (int device_count = 0; device_count <= device; device_count++)
		{
			// Check for requested device
			result = deckLinkIterator->Next(&deckLink);
			if (result != S_OK)
				throw DecklinkError("No DeckLink PCI cards found.");

			if (device_count == device)
				break;
		}

		if (deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput) != S_OK)
			throw DecklinkError("DeckLink QueryInterface Failed.");

		// Obtain an IDeckLinkDisplayModeIterator to enumerate the display modes supported on output
		result = deckLinkOutput->GetDisplayModeIterator(&displayModeIterator);
		if (result != S_OK)
			throw DecklinkError("Could not obtain the video output display mode iterator.");

		if (g_videoModeIndex < 0)
			throw DecklinkError("No video mode specified.");

		// Loop through all available display modes, until a match is found (if any)
		const char *displayModeName;
		BMDTimeValue frameRateDuration, frameRateScale;

		while (displayModeIterator->Next(&displayMode) == S_OK)
		{
			if (g_videoModeIndex == displayModeCount)
			{
				BMDDisplayModeSupport result;

				foundDisplayMode = true;
				displayMode->GetName(&displayModeName);
				selectedDisplayMode = displayMode->GetDisplayMode();
				//deckLinkOutput->DoesSupportVideoMode(selectedDisplayMode, pixelFormat, bmdVideoOutputFlagDefault, &result, NULL);

				// Get framerate
		        displayMode->GetFrameRate(&frameRateDuration, &frameRateScale);

				//if (result == bmdDisplayModeNotSupported)
				//{
				//	cout << "The display mode does not support the selected pixel format." << endl;
				//	throw DecklinkError("The display mode does not support the selected pixel format.");
				//}

				break;
			}
			displayModeCount++;
		}

		if (!foundDisplayMode)
			throw DecklinkError("Invalid video mode.  No matching ones found.");

		// Calculate FPS
		unsigned long m_framesPerSecond = (unsigned long)((frameRateScale + (frameRateDuration-1))  /  frameRateDuration);

		// Create Delegate & Pass in pointers to the output and converters
		delegate = new DeckLinkOutputDelegate(displayMode, deckLinkOutput);

		// Provide this class as a delegate to the audio and video output interfaces
		deckLinkOutput->SetScheduledFrameCompletionCallback(delegate);
		//deckLinkOutput->SetAudioCallback(delegate);

		// Check for video input
		if (deckLinkOutput->EnableVideoOutput(displayMode->GetDisplayMode(), bmdVideoOutputFlagDefault) != S_OK)
	    	throw DecklinkError("Failed to enable video output. Is another application using the card?");

	    // Check for audio input
		//if (deckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, g_audioSampleDepth, g_audioChannels, bmdAudioOutputStreamContinuous) != S_OK)
	    //	throw DecklinkError("Failed to enable audio output. Is another application using the card?");

		// Begin video preroll by scheduling a second of frames in hardware
		//std::shared_ptr<Frame> f(new Frame(1, displayMode->GetWidth(), displayMode->GetHeight(), "Blue"));
		//f->AddColor(displayMode->GetWidth(), displayMode->GetHeight(), "Blue");

		// Preroll 1 second of video
		//for (unsigned i = 0; i < 16; i++)
		//{
		//	// Write 30 blank frames (for preroll)
		//	delegate->WriteFrame(f);
		//	delegate->ScheduleNextFrame(true);
		//}

		//deckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
		//if (deckLinkOutput->BeginAudioPreroll() != S_OK)
		//	throw DecklinkError("Failed to begin audio preroll.");


		// Update image properties
		info.has_audio = true;
		info.has_video = true;
		info.vcodec = displayModeName;
		info.width = displayMode->GetWidth();
		info.height = displayMode->GetHeight();
		info.file_size = info.width * info.height * sizeof(char) * 4;
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = 60 * 60 * 24; // 24 hour duration... since we're capturing a live stream
		info.fps.num = frameRateScale;
		info.fps.den = frameRateDuration;
		info.video_timebase.num = frameRateDuration;
		info.video_timebase.den = frameRateScale;
		info.video_length = round(info.duration * info.fps.ToDouble());

		// Calculate the DAR (display aspect ratio)
		Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

		// Reduce size fraction
		size.Reduce();

		// Set the ratio based on the reduced fraction
		info.display_ratio.num = size.num;
		info.display_ratio.den = size.den;

		// Mark as "open"
		is_open = true;
	}
}

// Close device and video stream
void DecklinkWriter::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Stop the audio and video output streams immediately
		deckLinkOutput->StopScheduledPlayback(0, NULL, 0);
		deckLinkOutput->DisableAudioOutput();
		deckLinkOutput->DisableVideoOutput();

		// Release DisplayMode
		displayMode->Release();

		if (displayModeIterator != NULL)
		{
			displayModeIterator->Release();
			displayModeIterator = NULL;
		}

	    if (deckLinkOutput != NULL)
	    {
	    	deckLinkOutput->Release();
	    	deckLinkOutput = NULL;
	    }

	    if (deckLink != NULL)
	    {
	        deckLink->Release();
	        deckLink = NULL;
	    }

		if (deckLinkIterator != NULL)
			deckLinkIterator->Release();

		// Mark as "closed"
		is_open = false;
	}
}

// This method is required for all derived classes of WriterBase.  Write a Frame to the video file.
void DecklinkWriter::WriteFrame(std::shared_ptr<Frame> frame) throw(WriterClosed)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw WriterClosed("The DecklinkWriter is closed.  Call Open() before calling this method.", "");

	delegate->WriteFrame(frame);
}

// This method is required for all derived classes of WriterBase.  Write a block of frames from a reader.
void DecklinkWriter::WriteFrame(ReaderBase* reader, int start, int length) throw(WriterClosed)
{
	// Loop through each frame (and encoded it)
	for (int number = start; number <= length; number++)
	{
		// Get the frame
		std::shared_ptr<Frame> f = reader->GetFrame(number);

		// Encode frame
		WriteFrame(f);
	}
}
