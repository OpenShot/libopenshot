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

#ifndef OPENSHOT_FRAME_H
#define OPENSHOT_FRAME_H

/// Do not include the juce unittest headers, because it collides with unittest++
#ifndef __JUCE_UNITTEST_JUCEHEADER__
	#define __JUCE_UNITTEST_JUCEHEADER__
#endif
#ifndef _NDEBUG
	// Define NO debug for JUCE on mac os
	#define _NDEBUG
#endif

#include <iomanip>
#include <sstream>
#include <queue>
#include <QtWidgets/QApplication>
#include <QtGui/QImage>
#include <QtGui/QColor>
#include <QtGui/QBitmap>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QPainter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <memory>
#include <unistd.h>
#include "ZmqLogger.h"
#ifdef USE_IMAGEMAGICK
	#include "Magick++.h"
#endif
#include "JuceLibraryCode/JuceHeader.h"
#include "ChannelLayouts.h"
#include "AudioBufferSource.h"
#include "AudioResampler.h"
#include "Fraction.h"


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
	 * std::shared_ptr<Frame> f(new Frame(1, 720, 480, "#000000", 44100, 2));
	 *
	 * @endcode
	 */
	class Frame
	{
	private:
		std::shared_ptr<QImage> image;
		std::shared_ptr<QImage> wave_image;
		std::shared_ptr<juce::AudioSampleBuffer> audio;
		std::shared_ptr<QApplication> previewApp;
		CriticalSection addingImageSection;
        CriticalSection addingAudioSection;
		const unsigned char *qbuffer;
		Fraction pixel_ratio;
		int channels;
		ChannelLayout channel_layout;
		int width;
		int height;
		int sample_rate;

		/// Constrain a color value from 0 to 255
		int constrain(int color_value);

	public:
		long int number;	 ///< This is the frame number (starting at 1)
		bool has_audio_data; ///< This frame has been loaded with audio data
		bool has_image_data; ///< This frame has been loaded with pixel data

		/// Constructor - blank frame (300x200 blank image, 48kHz audio silence)
		Frame();

		/// Constructor - image only (48kHz audio silence)
		Frame(long int number, int width, int height, string color);

		/// Constructor - audio only (300x200 blank image)
		Frame(long int number, int samples, int channels);

		/// Constructor - image & audio
		Frame(long int number, int width, int height, string color, int samples, int channels);

		/// Copy constructor
		Frame ( const Frame &other );

		/// Assignment operator
		//Frame& operator= (const Frame& other);

		/// Destructor
		~Frame();

		/// Add (or replace) pixel data to the frame (based on a solid color)
		void AddColor(int new_width, int new_height, string color);

		/// Add (or replace) pixel data to the frame
		void AddImage(int new_width, int new_height, int bytes_per_pixel, QImage::Format type, const unsigned char *pixels_);

		/// Add (or replace) pixel data to the frame
		void AddImage(std::shared_ptr<QImage> new_image);

		/// Add (or replace) pixel data to the frame (for only the odd or even lines)
		void AddImage(std::shared_ptr<QImage> new_image, bool only_odd_lines);

#ifdef USE_IMAGEMAGICK
		/// Add (or replace) pixel data to the frame from an ImageMagick Image
		void AddMagickImage(std::shared_ptr<Magick::Image> new_image);
#endif

		/// Add audio samples to a specific channel
		void AddAudio(bool replaceSamples, int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource);

		/// Add audio silence
		void AddAudioSilence(int numSamples);

		/// Apply gain ramp (i.e. fading volume)
		void ApplyGainRamp(int destChannel, int destStartSample, int numSamples, float initial_gain, float final_gain);

		/// Channel Layout of audio samples. A frame needs to keep track of this, since Writers do not always
		/// know the original channel layout of a frame's audio samples (i.e. mono, stereo, 5 point surround, etc...)
		ChannelLayout ChannelsLayout();

		// Set the channel layout of audio samples (i.e. mono, stereo, 5 point surround, etc...)
		void ChannelsLayout(ChannelLayout new_channel_layout) { channel_layout = new_channel_layout; };

		/// Clean up buffer after QImage is deleted
		static void cleanUpBuffer(void *info);

		/// Clear the waveform image (and deallocate it's memory)
		void ClearWaveform();

		/// Copy data and pointers from another Frame instance
		void DeepCopy(const Frame& other);

		/// Display the frame image to the screen (primarily used for debugging reasons)
		void Display();

		/// Display the wave form
		void DisplayWaveform();

		/// Get magnitude of range of samples (if channel is -1, return average of all channels for that sample)
		float GetAudioSample(int channel, int sample, int magnitude_range);

		/// Get an array of sample data
		float* GetAudioSamples(int channel);

		/// Get an array of sample data (all channels interleaved together), using any sample rate
		float* GetInterleavedAudioSamples(int new_sample_rate, AudioResampler* resampler, int* sample_count);

		// Get a planar array of sample data, using any sample rate
		float* GetPlanarAudioSamples(int new_sample_rate, AudioResampler* resampler, int* sample_count);

		/// Get number of audio channels
		int GetAudioChannelsCount();

		/// Get number of audio samples
		int GetAudioSamplesCount();

	    juce::AudioSampleBuffer *GetAudioSampleBuffer();

		/// Get the size in bytes of this frame (rough estimate)
		int64 GetBytes();

		/// Get pointer to Qt QImage image object
		std::shared_ptr<QImage> GetImage();

#ifdef USE_IMAGEMAGICK
		/// Get pointer to ImageMagick image object
		std::shared_ptr<Magick::Image> GetMagickImage();
#endif

		/// Set Pixel Aspect Ratio
		Fraction GetPixelRatio() { return pixel_ratio; };

		/// Get pixel data (as packets)
		const unsigned char* GetPixels();

		/// Get pixel data (for only a single scan-line)
		const unsigned char* GetPixels(int row);

		/// Get height of image
		int GetHeight();

		/// Calculate the # of samples per video frame (for the current frame number)
		int GetSamplesPerFrame(Fraction fps, int sample_rate, int channels);

		/// Calculate the # of samples per video frame (for a specific frame number and frame rate)
		static int GetSamplesPerFrame(long int frame_number, Fraction fps, int sample_rate, int channels);

		/// Get an audio waveform image
		std::shared_ptr<QImage> GetWaveform(int width, int height, int Red, int Green, int Blue, int Alpha);

		/// Get an audio waveform image pixels
		const unsigned char* GetWaveformPixels(int width, int height, int Red, int Green, int Blue, int Alpha);

		/// Get height of image
		int GetWidth();

		/// Resize audio container to hold more (or less) samples and channels
		void ResizeAudio(int channels, int length, int sample_rate, ChannelLayout channel_layout);

		/// Get the original sample rate of this frame's audio data
		int SampleRate();

		/// Set the original sample rate of this frame's audio data
		void SampleRate(int orig_sample_rate) { sample_rate = orig_sample_rate; };

		/// Save the frame image to the specified path.  The image format can be BMP, JPG, JPEG, PNG, PPM, XBM, XPM
		void Save(string path, float scale, string format="PNG", int quality=100);

		/// Set frame number
		void SetFrameNumber(long int number);

		/// Set Pixel Aspect Ratio
		void SetPixelRatio(int num, int den);

		/// Thumbnail the frame image with tons of options to the specified path.  The image format is determined from the extension (i.e. image.PNG, image.JPEG).
		/// This method allows for masks, overlays, background color, and much more accurate resizing (including padding and centering)
		void Thumbnail(string path, int new_width, int new_height, string mask_path, string overlay_path,
				string background_color, bool ignore_aspect, string format="png", int quality=100) throw(InvalidFile);

		/// Play audio samples for this frame
		void Play();
	};

}

#endif
