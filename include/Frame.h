/**
 * @file
 * @brief Header file for Frame class
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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_FRAME_H
#define OPENSHOT_FRAME_H

/// Do not include the juce unittest headers, because it collides with unittest++
#ifndef __JUCE_UNITTEST_JUCEHEADER__
	#define __JUCE_UNITTEST_JUCEHEADER__
#endif

// Defining some ImageMagic flags... for Mac (since they appear to be unset)
#ifndef MAGICKCORE_QUANTUM_DEPTH
	#define MAGICKCORE_QUANTUM_DEPTH 16
#endif
#ifndef MAGICKCORE_HDRI_ENABLE
	#define MAGICKCORE_HDRI_ENABLE 0
#endif
#ifndef _NDEBUG
	// Define NO debug for JUCE on mac os
	#define _NDEBUG
#endif

#include <iomanip>
#include <sstream>
#include <queue>
#include <tr1/memory>
#include <unistd.h>
#include "Magick++.h"
#include "JuceLibraryCode/JuceHeader.h"
#include "AudioBufferSource.h"
#include "AudioResampler.h"
#include "Fraction.h"
#include "Sleep.h"

using namespace std;

namespace openshot
{
	/**
	 * @brief This class represents a single frame of video (i.e. image & audio data)
	 *
	 * FileReaders (such as FFmpegReader) use instances of this class to store the individual frames of video,
	 * which include both the image data (i.e. pixels) and audio samples. An openshot::Frame also has many debug
	 * methods, such as the ability to display the image (using X11), play the audio samples (using JUCE), or
	 * display the audio waveform as an image.
	 *
	 * FileWriters (such as FFmpegWriter) use instances of this class to create new video files, image files, or
	 * video streams. So, think of these openshot::Frame instances as the smallest unit of work in a video
	 * editor.
	 *
	 * There are many ways to create an instance of an openshot::Frame:
	 * @code
	 *
	 * // Most basic: a blank frame (300x200 blank image, 48kHz audio silence)
	 * Frame();
	 *
	 * // Image only settings (48kHz audio silence)
	 * Frame(1, // Frame number
	 *       720, // Width of image
	 *       480, // Height of image
	 *       "#000000" // HTML color code of background color
	 *       );
	 *
	 * // Image only from pixel array (48kHz audio silence)
	 * Frame(number, // Frame number
	 *       720, // Width of image
	 *       480, // Height of image
	 *       "RGBA", // Color format / map
	 *       Magick::CharPixel, // Storage format / data type
	 *       buffer // Array of image data (pixels)
	 *       );
	 *
	 * // Audio only (300x200 blank image)
	 * Frame(number, // Frame number
	 *       44100, // Sample rate of audio stream
	 *       2 // Number of audio channels
	 *       );
	 *
	 * // Image and Audio settings (user defines all key settings)
	 * Frame(number, // Frame number
	 *       720, // Width of image
	 *       480, // Height of image
	 *       "#000000" // HTML color code of background color
	 *       44100, // Sample rate of audio stream
	 *       2 // Number of audio channels
	 *       );
	 *
	 * // Some methods require a shared pointer to an openshot::Frame object.
	 * tr1::shared_ptr<Frame> f(new Frame(1, 720, 480, "#000000", 44100, 2));
	 *
	 * @endcode
	 */
	class Frame
	{
	private:
		tr1::shared_ptr<Magick::Image> image;
		tr1::shared_ptr<Magick::Image> wave_image;
		tr1::shared_ptr<juce::AudioSampleBuffer> audio;
		Fraction pixel_ratio;
		int channels;
		int width;
		int height;

	public:
		int number;	///< This is the frame number (starting at 1)

		/// Constructor - blank frame (300x200 blank image, 48kHz audio silence)
		Frame();

		/// Constructor - image only (48kHz audio silence)
		Frame(int number, int width, int height, string color);

		/// Constructor - image only from pixel array (48kHz audio silence)
		Frame(int number, int width, int height, const string map, const Magick::StorageType type, const void *pixels_);

		/// Constructor - audio only (300x200 blank image)
		Frame(int number, int samples, int channels);

		/// Constructor - image & audio
		Frame(int number, int width, int height, string color, int samples, int channels);

		/// Copy constructor
		Frame ( const Frame &other );

		/// Assignment operator
		//Frame& operator= (const Frame& other);

		/// Destructor
		~Frame();

		/// Add (or replace) pixel data to the frame (based on a solid color)
		void AddColor(int width, int height, string color);

		/// Add (or replace) pixel data to the frame
		void AddImage(int width, int height, const string map, const Magick::StorageType type, const void *pixels_);

		/// Add (or replace) pixel data to the frame
		void AddImage(tr1::shared_ptr<Magick::Image> new_image);

		/// Add (or replace) pixel data to the frame (for only the odd or even lines)
		void AddImage(tr1::shared_ptr<Magick::Image> new_image, bool only_odd_lines);

		/// Add audio samples to a specific channel
		void AddAudio(bool replaceSamples, int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource);

		/// Apply gain ramp (i.e. fading volume)
		void ApplyGainRamp(int destChannel, int destStartSample, int numSamples, float initial_gain, float final_gain);

		/// Composite a new image on top of the existing image
		void AddImage(tr1::shared_ptr<Magick::Image> new_image, float alpha);

		/// Experimental method to add effects to this frame
		void AddEffect(string name);

		/// Experimental method to add overlay images to this frame
		void AddOverlay(Frame* frame);

		/// Experimental method to add the frame number on top of the image
		void AddOverlayNumber(int overlay_number);

		/// Clear the waveform image (and deallocate it's memory)
		void ClearWaveform();

		/// Copy data and pointers from another Frame instance
		void DeepCopy(const Frame& other);

		/// Display the frame image to the screen (primarily used for debugging reasons)
		void Display();

		/// Display the wave form
		void DisplayWaveform();

		/// Get an array of sample data
		float* GetAudioSamples(int channel);

		/// Get an array of sample data (all channels interleaved together), using any sample rate
		float* GetInterleavedAudioSamples(int original_sample_rate, int new_sample_rate, AudioResampler* resampler, int* sample_count);

		/// Get number of audio channels
		int GetAudioChannelsCount();

		/// Get number of audio channels
		int GetAudioSamplesCount();

	    juce::AudioSampleBuffer *GetAudioSampleBuffer();

		/// Get the size in bytes of this frame (rough estimate)
		int64 GetBytes();

		/// Get pointer to Magick++ image object
		tr1::shared_ptr<Magick::Image> GetImage();

		/// Get pixel data (as packets)
		const Magick::PixelPacket* GetPixels();

		/// Get pixel data (for only a single scan-line)
		const Magick::PixelPacket* GetPixels(int row);

		/// Get height of image
		int GetHeight();

		/// Calculate the # of samples per video frame (for the current frame number)
		int GetSamplesPerFrame(Fraction fps, int sample_rate);

		/// Calculate the # of samples per video frame (for a specific frame number and frame rate)
		static int GetSamplesPerFrame(int frame_number, Fraction fps, int sample_rate);

		/// Get an audio waveform image
		tr1::shared_ptr<Magick::Image> GetWaveform(int width, int height, int Red, int Green, int Blue);

		/// Get an audio waveform image pixels
		const Magick::PixelPacket* GetWaveformPixels(int width, int height, int Red, int Green, int Blue);

		/// Get height of image
		int GetWidth();

		/// Rotate the image
		void Rotate(float degrees);

		/// Save the frame image to the specified path.  The image format is determined from the extension (i.e. image.PNG, image.JPEG)
		void Save(string path, float scale);

		/// Set frame number
		void SetFrameNumber(int number);

		/// Set Pixel Aspect Ratio
		void SetPixelRatio(int num, int den);

		/// Make colors in a specific range transparent
		void TransparentColors(string color, double fuzz);

		/// Play audio samples for this frame
		void Play(int sample_rate);
	};

}

#endif
