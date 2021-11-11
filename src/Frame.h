/**
 * @file
 * @brief Header file for Frame class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_FRAME_H
#define OPENSHOT_FRAME_H

#ifdef USE_OPENCV
	#define int64 opencv_broken_int
	#define uint64 opencv_broken_uint
	#include <opencv2/imgproc/imgproc.hpp>
	#undef uint64
	#undef int64
#endif

#include <memory>
#include <mutex>
#include <sstream>
#include <queue>

#include "ChannelLayouts.h"
#include "Fraction.h"

#include <QColor>
#include <QImage>

class QApplication;

namespace juce {
    template <typename Type> class AudioBuffer;
}

namespace openshot
{
	class AudioBufferSource;
	class AudioResampler;
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
	 * // Most basic: a blank frame (all default values)
	 * Frame();
	 *
	 * // Image only settings
	 * Frame(1, // Frame number
	 *       720, // Width of image
	 *       480, // Height of image
	 *       "#000000" // HTML color code of background color
	 *       );
	 *
	 * // Audio only
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
	 * auto f = std::make_shared<openshot::Frame>(1, 720, 480, "#000000", 44100, 2);
	 *
	 * @endcode
	 */
	class Frame
	{
	private:
		std::shared_ptr<QImage> image;
		std::shared_ptr<QImage> wave_image;

		std::shared_ptr<QApplication> previewApp;
		std::recursive_mutex addingImageMutex;
		std::recursive_mutex addingAudioMutex;
		openshot::Fraction pixel_ratio;
		int channels;
		ChannelLayout channel_layout;
		int width;
		int height;
		int sample_rate;
		std::string color;
		int64_t max_audio_sample; ///< The max audio sample count added to this frame

#ifdef USE_OPENCV
		cv::Mat imagecv; ///< OpenCV image. It will always be in BGR format
#endif

		/// Constrain a color value from 0 to 255
		int constrain(int color_value);

	public:
		std::shared_ptr<juce::AudioBuffer<float>> audio;
		int64_t number;	 ///< This is the frame number (starting at 1)
		bool has_audio_data; ///< This frame has been loaded with audio data
		bool has_image_data; ///< This frame has been loaded with pixel data


		/// Constructor - blank frame
		Frame();

		/// Constructor - image only
		Frame(int64_t number, int width, int height, std::string color);

		/// Constructor - audio only
		Frame(int64_t number, int samples, int channels);

		/// Constructor - image & audio
		Frame(int64_t number, int width, int height, std::string color, int samples, int channels);

		/// Copy constructor
		Frame ( const Frame &other );

		/// Assignment operator
		Frame& operator= (const Frame& other);

		/// Destructor
		virtual ~Frame();

		/// Add (or replace) pixel data to the frame (based on a solid color)
		void AddColor(int new_width, int new_height, std::string new_color);

		/// Add (or replace) pixel data (filled with new_color)
		void AddColor(const QColor& new_color);

		/// Add (or replace) pixel data to the frame
		void AddImage(int new_width, int new_height, int bytes_per_pixel, QImage::Format type, const unsigned char *pixels_);

		/// Add (or replace) pixel data to the frame
		void AddImage(std::shared_ptr<QImage> new_image);

		/// Add (or replace) pixel data to the frame (for only the odd or even lines)
		void AddImage(std::shared_ptr<QImage> new_image, bool only_odd_lines);

		/// Add audio samples to a specific channel
		void AddAudio(bool replaceSamples, int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource);

		/// Add audio silence
		void AddAudioSilence(int numSamples);

		/// Apply gain ramp (i.e. fading volume)
		void ApplyGainRamp(int destChannel, int destStartSample, int numSamples, float initial_gain, float final_gain);

		/// Channel Layout of audio samples. A frame needs to keep track of this, since Writers do not always
		/// know the original channel layout of a frame's audio samples (i.e. mono, stereo, 5 point surround, etc...)
		openshot::ChannelLayout ChannelsLayout();

		// Set the channel layout of audio samples (i.e. mono, stereo, 5 point surround, etc...)
		void ChannelsLayout(openshot::ChannelLayout new_channel_layout) { channel_layout = new_channel_layout; };

		/// Clear the waveform image (and deallocate its memory)
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
		float* GetInterleavedAudioSamples(int new_sample_rate, openshot::AudioResampler* resampler, int* sample_count);

		// Get a planar array of sample data, using any sample rate
		float* GetPlanarAudioSamples(int new_sample_rate, openshot::AudioResampler* resampler, int* sample_count);

		/// Get number of audio channels
		int GetAudioChannelsCount();

		/// Get number of audio samples
		int GetAudioSamplesCount();

	    juce::AudioBuffer<float> *GetAudioSampleBuffer();

		/// Get the size in bytes of this frame (rough estimate)
		int64_t GetBytes();

		/// Get pointer to Qt QImage image object
		std::shared_ptr<QImage> GetImage();

		/// Set Pixel Aspect Ratio
		openshot::Fraction GetPixelRatio() { return pixel_ratio; };

		/// Get pixel data (as packets)
		const unsigned char* GetPixels();

		/// Get pixel data (for only a single scan-line)
		const unsigned char* GetPixels(int row);

		/// Check a specific pixel color value (returns True/False)
		bool CheckPixel(int row, int col, int red, int green, int blue, int alpha, int threshold);

		/// Get height of image
		int GetHeight();

		/// Calculate the # of samples per video frame (for the current frame number)
		int GetSamplesPerFrame(openshot::Fraction fps, int sample_rate, int channels);

		/// Calculate the # of samples per video frame (for a specific frame number and frame rate)
		static int GetSamplesPerFrame(int64_t frame_number, openshot::Fraction fps, int sample_rate, int channels);

		/// Get an audio waveform image
		std::shared_ptr<QImage> GetWaveform(int width, int height, int Red, int Green, int Blue, int Alpha);

		/// Get an audio waveform image pixels
		const unsigned char* GetWaveformPixels(int width, int height, int Red, int Green, int Blue, int Alpha);

		/// Get height of image
		int GetWidth();

		/// Resize audio container to hold more (or less) samples and channels
		void ResizeAudio(int channels, int length, int sample_rate, openshot::ChannelLayout channel_layout);

		/// Get the original sample rate of this frame's audio data
		int SampleRate();

		/// Set the original sample rate of this frame's audio data
		void SampleRate(int orig_sample_rate) { sample_rate = orig_sample_rate; };

		/// Save the frame image to the specified path.  The image format can be BMP, JPG, JPEG, PNG, PPM, XBM, XPM
		void Save(std::string path, float scale, std::string format="PNG", int quality=100);

		/// Set frame number
		void SetFrameNumber(int64_t number);

		/// Set Pixel Aspect Ratio
		void SetPixelRatio(int num, int den);

		/// Thumbnail the frame image with tons of options to the specified path.  The image format is determined from the extension (i.e. image.PNG, image.JPEG).
		/// This method allows for masks, overlays, background color, and much more accurate resizing (including padding and centering)
		void Thumbnail(std::string path, int new_width, int new_height, std::string mask_path, std::string overlay_path,
				std::string background_color, bool ignore_aspect, std::string format="png", int quality=100, float rotate=0.0);

		/// Play audio samples for this frame
		void Play();

#ifdef USE_OPENCV
		/// Convert Qimage to Mat
		cv::Mat Qimage2mat( std::shared_ptr<QImage>& qimage);

		/// Convert OpenCV Mat to QImage
		std::shared_ptr<QImage> Mat2Qimage(cv::Mat img);

		/// Get pointer to OpenCV Mat image object
		cv::Mat GetImageCV();

		/// Set pointer to OpenCV image object
		void SetImageCV(cv::Mat _image);
#endif
	};

}

#endif
