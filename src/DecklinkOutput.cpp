/**
 * @file
 * @brief Source file for DecklinkOutput class
 * @author Jonathan Thomas <jonathan@openshot.org>, Blackmagic Design
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2009 Blackmagic Design
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *
 * Copyright (c) 2008-2020 OpenShot Studios, LLC
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

#include "../include/DecklinkOutput.h"

using namespace std;

DeckLinkOutputDelegate::DeckLinkOutputDelegate(IDeckLinkDisplayMode *displayMode, IDeckLinkOutput* m_deckLinkOutput)
 : m_refCount(0), displayMode(displayMode), width(0), height(0)
{
	// reference to output device
	deckLinkOutput = m_deckLinkOutput;

	// init some variables
	m_totalFramesScheduled = 0;
	m_audioChannelCount = 2;
	m_audioSampleRate = bmdAudioSampleRate48kHz;
	m_audioSampleDepth = 16;
	m_outputSignal = kOutputSignalDrop;
	m_currentFrame = NULL;

	// Get framerate
    displayMode->GetFrameRate(&frameRateDuration, &frameRateScale);
	m_framesPerSecond = (unsigned long)((frameRateScale + (frameRateDuration-1))  /  frameRateDuration);

	// Allocate audio array
	m_audioBufferSampleLength = (unsigned long)((m_framesPerSecond * m_audioSampleRate * frameRateDuration) / frameRateScale);
	m_audioBuffer = valloc(m_audioBufferSampleLength * m_audioChannelCount * (m_audioSampleDepth / 8));

	// Zero the buffer (interpreted as audio silence)
	memset(m_audioBuffer, 0x0, (m_audioBufferSampleLength * m_audioChannelCount * m_audioSampleDepth/8));
	audioSamplesPerFrame = (unsigned long)((m_audioSampleRate * frameRateDuration) / frameRateScale);

	pthread_mutex_init(&m_mutex, NULL);
}

DeckLinkOutputDelegate::~DeckLinkOutputDelegate()
{
	cout << "DESTRUCTOR!!!" << endl;
	pthread_mutex_destroy(&m_mutex);
}

/************************* DeckLink API Delegate Methods *****************************/
HRESULT DeckLinkOutputDelegate::ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	//cout << "Scheduled Successfully!" << endl;

	// When a video frame has been released by the API, schedule another video frame to be output
	ScheduleNextFrame(false);

	return S_OK;
}

HRESULT DeckLinkOutputDelegate::ScheduledPlaybackHasStopped ()
{
	//cout << "PLAYBACK HAS STOPPED!!!" << endl;
	return S_OK;
}

HRESULT DeckLinkOutputDelegate::RenderAudioSamples (bool preroll)
{
//	// Provide further audio samples to the DeckLink API until our preferred buffer waterlevel is reached
//	const unsigned long kAudioWaterlevel = 48000;
//	unsigned int bufferedSamples;
//
//	// Try to maintain the number of audio samples buffered in the API at a specified waterlevel
//	if ((deckLinkOutput->GetBufferedAudioSampleFrameCount(&bufferedSamples) == S_OK) && (bufferedSamples < kAudioWaterlevel))
//	{
//		unsigned int samplesToEndOfBuffer;
//		unsigned int samplesToWrite;
//		unsigned int samplesWritten;
//
//		samplesToEndOfBuffer = (m_audioBufferSampleLength - m_audioBufferOffset);
//		samplesToWrite = (kAudioWaterlevel - bufferedSamples);
//		if (samplesToWrite > samplesToEndOfBuffer)
//			samplesToWrite = samplesToEndOfBuffer;
//
//		if (deckLinkOutput->ScheduleAudioSamples((void*)((unsigned long)m_audioBuffer + (m_audioBufferOffset * m_audioChannelCount * m_audioSampleDepth/8)), samplesToWrite, 0, 0, &samplesWritten) == S_OK)
//		{
//			m_audioBufferOffset = ((m_audioBufferOffset + samplesWritten) % m_audioBufferSampleLength);
//		}
//	}
//
//
//	if (preroll)
//	{
//		// Start audio and video output
//		deckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
//	}

	return S_OK;
}

// Schedule the next frame
void DeckLinkOutputDelegate::ScheduleNextFrame(bool prerolling)
{
		// Get oldest frame (if any)
		if (final_frames.size() > 0)
		{
			#pragma omp critical (blackmagic_output_queue)
			{
				// Get the next frame off the queue
				uint8_t* castBytes = final_frames.front();
				final_frames.pop_front(); // remove this frame from the queue

				// Release the current frame (if any)
				if (m_currentFrame)
				{
					m_currentFrame->Release();
					m_currentFrame = NULL;
				}

				// Create a new one
				while (deckLinkOutput->CreateVideoFrame(
										width,
										height,
										width * 4,
										bmdFormat8BitARGB,
										bmdFrameFlagDefault,
										&m_currentFrame) != S_OK)
				{
					cout << "failed to create video frame" << endl;
					usleep(1000 * 1);
				}

				// Copy pixel data to frame
				void *frameBytesDest;
				m_currentFrame->GetBytes(&frameBytesDest);
				memcpy(frameBytesDest, castBytes, width * height * 4);

				// Delete temp array
				delete[] castBytes;
				castBytes = NULL;

			} // critical
		}
		//else
		//	cout << "Queue: empty on writer..." << endl;

		// Schedule a frame to be displayed
		if (m_currentFrame && deckLinkOutput->ScheduleVideoFrame(m_currentFrame, (m_totalFramesScheduled * frameRateDuration), frameRateDuration, frameRateScale) != S_OK)
			cout << "ScheduleVideoFrame FAILED!!! " << m_totalFramesScheduled << endl;

		// Update the timestamp (regardless of previous frame's success)
		m_totalFramesScheduled += 1;

}

void DeckLinkOutputDelegate::WriteFrame(std::shared_ptr<openshot::Frame> frame)
{

	#pragma omp critical (blackmagic_output_queue)
	// Add raw OpenShot frame object
	raw_video_frames.push_back(frame);


	// Process frames once we have a few (to take advantage of multiple threads)
	if (raw_video_frames.size() >= OPEN_MP_NUM_PROCESSORS)
	{

		//omp_set_num_threads(1);
		omp_set_nested(true);
		#pragma omp parallel
		{
		#pragma omp single
		{
			// Temp frame counters (to keep the frames in order)
			frameCount = 0;

			// Loop through each queued image frame
			while (!raw_video_frames.empty())
			{
				// Get front frame (from the queue)
				std::shared_ptr<openshot::Frame> frame = raw_video_frames.front();
				raw_video_frames.pop_front();

				// copy of frame count
				unsigned long copy_frameCount(frameCount);

				#pragma omp task firstprivate(frame, copy_frameCount)
				{
					// *********** CONVERT YUV source frame to RGB ************
					void *frameBytes;
					void *audioFrameBytes;

					width = frame->GetWidth();
					height = frame->GetHeight();

					// Get RGB Byte array
					int numBytes = frame->GetHeight() * frame->GetWidth() * 4;
					uint8_t *castBytes = new uint8_t[numBytes];

					// TODO: Fix Decklink support with QImage Upgrade
					// Get a list of pixels in our frame's image.  Each pixel is represented by
					// a PixelPacket struct, which has 4 properties: .red, .blue, .green, .alpha
//					const Magick::PixelPacket *pixel_packets = frame->GetPixels();
//
//					// loop through ImageMagic pixel structs, and put the colors in a regular array, and move the
//					// colors around to match the Decklink order (ARGB).
//					for (int packet = 0, row = 0; row < numBytes; packet++, row+=4)
//					{
//						// Update buffer (which is already linked to the AVFrame: pFrameRGB)
//						// Each color needs to be scaled to 8 bit (using the ImageMagick built-in ScaleQuantumToChar function)
//						castBytes[row] = MagickCore::ScaleQuantumToChar((Magick::Quantum) 0); // alpha
//						castBytes[row+1] = MagickCore::ScaleQuantumToChar((Magick::Quantum) pixel_packets[packet].red);
//						castBytes[row+2] = MagickCore::ScaleQuantumToChar((Magick::Quantum) pixel_packets[packet].green);
//						castBytes[row+3] = MagickCore::ScaleQuantumToChar((Magick::Quantum) pixel_packets[packet].blue);
//					}

					#pragma omp critical (blackmagic_output_queue)
					{
						//if (20 == frame->number)
						//	frame->Display();
						// Add processed frame to cache (to be recalled in order after the thread pool is done)
						temp_cache[copy_frameCount] = castBytes;
					}

				} // end task

				// Increment frame count
				frameCount++;

			} // end while
		} // omp single
		} // omp parallel


		// Add frames to final queue (in order)
		#pragma omp critical (blackmagic_output_queue)
		for (int z = 0; z < frameCount; z++)
		{
			// Add to final queue
			final_frames.push_back(temp_cache[z]);
		}

		// Clear temp cache
		temp_cache.clear();


		//cout << "final_frames.size(): " << final_frames.size() << ", raw_video_frames.size(): " << raw_video_frames.size() << endl;
		if (final_frames.size() >= m_framesPerSecond && m_totalFramesScheduled == 0)
		{
			cout << "Prerolling!" << endl;

			for (int x = 0; x < final_frames.size(); x++)
				ScheduleNextFrame(true);

			cout << "Starting scheduled playback!" << endl;

			// Start playback when enough frames have been processed
			deckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
		}
		else
		{
			// Be sure we don't have too many extra frames
			#pragma omp critical (blackmagic_output_queue)
			while (final_frames.size() > (m_framesPerSecond + 15))
			{
				//cout << "Too many, so remove some..." << endl;
				// Remove oldest frame
				delete[] final_frames.front();
				final_frames.pop_front();
			}
		}


	} // if

}
