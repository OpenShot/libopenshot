/**
 * @file
 * @brief Source file for DecklinkReader class
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

#include "../include/DecklinkReader.h"

using namespace openshot;

DecklinkReader::DecklinkReader(int device, int video_mode, int pixel_format, int channels, int sample_depth) throw(DecklinkError)
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
		default:
			throw DecklinkError("Pixel format is not valid (must be 0,1,2).");
	}


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

	if (deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&deckLinkInput) != S_OK)
		throw DecklinkError("DeckLink QueryInterface Failed.");

	// Obtain an IDeckLinkDisplayModeIterator to enumerate the display modes supported on output
	result = deckLinkInput->GetDisplayModeIterator(&displayModeIterator);
	if (result != S_OK)
		throw DecklinkError("Could not obtain the video output display mode iterator.");

	// Init deckLinkOutput (needed for color conversion)
	if (deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&m_deckLinkOutput) != S_OK)
		throw DecklinkError("Failed to create a deckLinkOutput(), used to convert YUV to RGB.");

	// Init the YUV to RGB conversion
	if(!(m_deckLinkConverter = CreateVideoConversionInstance()))
		throw DecklinkError("Failed to create a VideoConversionInstance(), used to convert YUV to RGB.");

	// Create Delegate & Pass in pointers to the output and converters
	delegate = new DeckLinkInputDelegate(&sleepCond, m_deckLinkOutput, m_deckLinkConverter);
	deckLinkInput->SetCallback(delegate);



	if (g_videoModeIndex < 0)
		throw DecklinkError("No video mode specified.");

	// Loop through all available display modes, until a match is found (if any)
	while (displayModeIterator->Next(&displayMode) == S_OK)
	{
		if (g_videoModeIndex == displayModeCount)
		{
			BMDDisplayModeSupport result;

			foundDisplayMode = true;
			displayMode->GetName(&displayModeName);
			selectedDisplayMode = displayMode->GetDisplayMode();
			deckLinkInput->DoesSupportVideoMode(selectedDisplayMode, pixelFormat, bmdVideoInputFlagDefault, &result, NULL);

			// Get framerate
	        displayMode->GetFrameRate(&frameRateDuration, &frameRateScale);

			if (result == bmdDisplayModeNotSupported)
				throw DecklinkError("The display mode does not support the selected pixel format.");

			if (inputFlags & bmdVideoInputDualStream3D)
			{
				if (!(displayMode->GetFlags() & bmdDisplayModeSupports3D))
					throw DecklinkError("The display mode does not support 3D.");
			}

			break;
		}
		displayModeCount++;
		displayMode->Release();
	}

	if (!foundDisplayMode)
		throw DecklinkError("Invalid video mode.  No matching ones found.");

	// Check for video input
    result = deckLinkInput->EnableVideoInput(selectedDisplayMode, pixelFormat, inputFlags);
    if(result != S_OK)
    	throw DecklinkError("Failed to enable video input. Is another application using the card?");

    // Check for audio input
    result = deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz, g_audioSampleDepth, g_audioChannels);
    if(result != S_OK)
    	throw DecklinkError("Failed to enable audio input. Is another application using the card?");

}

// destructor
DecklinkReader::~DecklinkReader()
{
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
}

// Open image file
void DecklinkReader::Open() throw(DecklinkError)
{
	// Open reader if not already open
	if (!is_open)
	{
	    // Start the streams
		result = deckLinkInput->StartStreams();
	    if(result != S_OK)
	    	throw DecklinkError("Failed to start the video and audio streams.");


		// Update image properties
		info.has_audio = false;
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
void DecklinkReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Stop streams
		result = deckLinkInput->StopStreams();

	    if(result != S_OK)
	    	throw DecklinkError("Failed to stop the video and audio streams.");

		// Mark as "closed"
		is_open = false;
	}
}

unsigned long DecklinkReader::GetCurrentFrameNumber()
{
	return delegate->GetCurrentFrameNumber();
}

// Get an openshot::Frame object for the next available LIVE frame
std::shared_ptr<Frame> DecklinkReader::GetFrame(long int requested_frame) throw(ReaderClosed)
{
	// Get a frame from the delegate decklink class (which is collecting them on another thread)
	std::shared_ptr<Frame> f = delegate->GetFrame(requested_frame);

//	cout << "Change the frame number to " << requested_frame << endl;
//	f->SetFrameNumber(requested_frame);
	return f; // frame # does not matter, since it always gets the oldest frame
}


// Generate JSON string of this object
string DecklinkReader::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::JsonValue for this object
Json::Value DecklinkReader::JsonValue() {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "DecklinkReader";

	// return JsonValue
	return root;
}

// Load JSON string into this object
void DecklinkReader::SetJson(string value) throw(InvalidJSON) {

	// Parse JSON string into JSON objects
	Json::Value root;
	Json::Reader reader;
	bool success = reader.parse( value, root );
	if (!success)
		// Raise exception
		throw InvalidJSON("JSON could not be parsed (or is invalid)", "");

	try
	{
		// Set all values that match
		SetJsonValue(root);
	}
	catch (exception e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)", "");
	}
}

// Load Json::JsonValue into this object
void DecklinkReader::SetJsonValue(Json::Value root) throw(InvalidFile) {

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Re-Open path, and re-init everything (if needed)
	if (is_open)
	{
		Close();
		Open();
	}
}
