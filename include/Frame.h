#ifndef OPENSHOT_FRAME_H
#define OPENSHOT_FRAME_H

/**
 * \file
 * \brief Header file for Frame class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

/// Do not include the juce unittest headers, because it collides with unittest++
#ifndef __JUCE_UNITTEST_JUCEHEADER__
	#define __JUCE_UNITTEST_JUCEHEADER__
#endif

#include <iomanip>
#include <sstream>
#include <queue>
#include "Magick++.h"
#include "JuceLibraryCode/JuceHeader.h"
#include "AudioBufferSource.h"
#include "AudioResampler.h"
#include "Fraction.h"

using namespace std;

namespace openshot
{
	/**
	 * \brief This class represents a single frame of video (i.e. image & audio data)
	 *
	 * FileReaders (such as FFmpegReader) use instances of this class to store the individual frames of video,
	 * which include both the image data (i.e. pixels) and audio samples.
	 */
	class Frame
	{
	private:
		Magick::Image *image;
		Magick::Image *small_image;
		Magick::Image *wave_image;
		juce::AudioSampleBuffer *audio;
		Fraction pixel_ratio;
		int sample_rate;
		int channels;

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

		/// Destructor
		~Frame();

		/// Copy constructor
		Frame ( const Frame &other );

		/// Assignment operator
		Frame& operator= (const Frame& other);

		/// Add (or replace) pixel data to the frame
		void AddImage(int width, int height, const string map, const Magick::StorageType type, const void *pixels_);

		/// Add (or replace) pixel data to the frame
		void AddImage(Magick::Image* new_image);

		/// Add audio samples to a specific channel
		void AddAudio(int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource);

		/// Experimental method to add effects to this frame
		void AddEffect(string name);

		/// Clear the waveform image (and deallocate it's memory)
		void ClearWaveform();

		/// Copy data and pointers from another Frame instance
		void DeepCopy(const Frame& other);

		/// Deallocate image and audio memory
		void DeletePointers();

		/// Display the frame image to the screen (primarily used for debugging reasons)
		void Display();

		/// Display the wave form
		void DisplayWaveform();

		/// Get an array of sample data
		float* GetAudioSamples(int channel);

		/// Get an array of sample data (all channels interleaved together), using any sample rate
		float* GetInterleavedAudioSamples(int new_sample_rate, AudioResampler* resampler, int* sample_count);

		/// Get number of audio channels
		int GetAudioChannelsCount();

		/// Get number of audio channels
		int GetAudioSamplesCount();

		/// Get the audio sample rate
		int GetAudioSamplesRate();

		/// Get pixel data (as packets)
		const Magick::PixelPacket* GetPixels();

		/// Get pixel data (for only a single scan-line)
		const Magick::PixelPacket* GetPixels(int row);

		/// Get pixel data (for a resized image)
		const Magick::PixelPacket* GetPixels(unsigned int width, unsigned int height, int frame);

		/// Get height of image
		int GetHeight();

		/// Get an audio waveform image
		Magick::Image* GetWaveform(int width, int height);

		/// Get an audio waveform image pixels
		const Magick::PixelPacket* GetWaveformPixels(int width, int height);

		/// Get height of image
		int GetWidth();

		/// Save the frame image to the specified path.  The image format is determined from the extension (i.e. image.PNG, image.JPEG)
		void Save(string path, float scale);

		/// Set Pixel Aspect Ratio
		void SetPixelRatio(int num, int den);

		/// Set Sample Rate, used for playback (Play() method)
		void SetSampleRate(int sample_rate);

		/// Play audio samples for this frame
		void Play();
	};

}

#endif
