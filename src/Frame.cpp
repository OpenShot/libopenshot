/**
 * @file
 * @brief Source file for Frame class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <thread>    // for std::this_thread::sleep_for
#include <chrono>    // for std::chrono::milliseconds
#include <sstream>
#include <iomanip>

#include "Frame.h"
#include "AudioBufferSource.h"
#include "AudioResampler.h"
#include "QtUtilities.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QBitmap>
#include <QColor>
#include <QString>
#include <QVector>
#include <QPainter>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPointF>
#include <QWidget>

using namespace std;
using namespace openshot;

// Constructor - image & audio
Frame::Frame(int64_t number, int width, int height, std::string color, int samples, int channels)
	: audio(std::make_shared<juce::AudioBuffer<float>>(channels, samples)),
	  number(number), width(width), height(height),
	  pixel_ratio(1,1), color(color),
	  channels(channels), channel_layout(LAYOUT_STEREO),
	  sample_rate(44100),
	  has_audio_data(false), has_image_data(false),
	  max_audio_sample(0)
{
	// zero (fill with silence) the audio buffer
	audio->clear();
}

// Delegating Constructor - blank frame
Frame::Frame() : Frame::Frame(1, 1, 1, "#000000", 0, 2) {}

// Delegating Constructor - image only
Frame::Frame(int64_t number, int width, int height, std::string color)
	: Frame::Frame(number, width, height, color, 0, 2) {}

// Delegating Constructor - audio only
Frame::Frame(int64_t number, int samples, int channels)
	: Frame::Frame(number, 1, 1, "#000000", samples, channels) {}


// Copy constructor
Frame::Frame ( const Frame &other )
{
	// copy pointers and data
	DeepCopy(other);
}

// Assignment operator
Frame& Frame::operator= (const Frame& other)
{
	// copy pointers and data
	DeepCopy(other);

	return *this;
}

// Copy data and pointers from another Frame instance
void Frame::DeepCopy(const Frame& other)
{
	number = other.number;
	channels = other.channels;
	width = other.width;
	height = other.height;
	channel_layout = other.channel_layout;
	has_audio_data = other.has_audio_data;
	has_image_data = other.has_image_data;
	sample_rate = other.sample_rate;
	pixel_ratio = Fraction(other.pixel_ratio.num, other.pixel_ratio.den);
	color = other.color;
	max_audio_sample = other.max_audio_sample;

	if (other.image)
		image = std::make_shared<QImage>(*(other.image));
	if (other.audio)
		audio = std::make_shared<juce::AudioBuffer<float>>(*(other.audio));
	if (other.wave_image)
		wave_image = std::make_shared<QImage>(*(other.wave_image));
}

// Destructor
Frame::~Frame() {
	// Clear all pointers
	image.reset();
	audio.reset();
	#ifdef USE_OPENCV
	imagecv.release();
	#endif
}

// Display the frame image to the screen (primarily used for debugging reasons)
void Frame::Display()
{
	if (!QApplication::instance()) {
		// Only create the QApplication once
		static int argc = 1;
		static char* argv[1] = {NULL};
		previewApp = std::make_shared<QApplication>(argc, argv);
	}

	// Get preview image
	std::shared_ptr<QImage> previewImage = GetImage();

    // Update the image to reflect the correct pixel aspect ration (i.e. to fix non-square pixels)
    if (pixel_ratio.num != 1 || pixel_ratio.den != 1)
    {
        // Resize to fix DAR
        previewImage = std::make_shared<QImage>(previewImage->scaled(
                previewImage->size().width(), previewImage->size().height() * pixel_ratio.Reciprocal().ToDouble(),
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

	// Create window
	QWidget previewWindow;
	previewWindow.setStyleSheet("background-color: #000000;");
	QHBoxLayout layout;

	// Create label with current frame's image
	QLabel previewLabel;
	previewLabel.setPixmap(QPixmap::fromImage(*previewImage));
	previewLabel.setMask(QPixmap::fromImage(*previewImage).mask());
	layout.addWidget(&previewLabel);

	// Show the window
	previewWindow.setLayout(&layout);
	previewWindow.show();
	previewApp->exec();
}

// Get an audio waveform image
std::shared_ptr<QImage> Frame::GetWaveform(int width, int height, int Red, int Green, int Blue, int Alpha)
{
	// Clear any existing waveform image
	ClearWaveform();

	// Init a list of lines
	QVector<QPointF> lines;
	QVector<QPointF> labels;

	// Calculate width of an image based on the # of samples
	int total_samples = GetAudioSamplesCount();
	if (total_samples > 0)
	{
		// If samples are present...
		int new_height = 200 * audio->getNumChannels();
		int height_padding = 20 * (audio->getNumChannels() - 1);
		int total_height = new_height + height_padding;
		int total_width = 0;
		float zero_height = 1.0; // Used to clamp near-zero vales to this value to prevent gaps

		// Loop through each audio channel
		float Y = 100.0;
		for (int channel = 0; channel < audio->getNumChannels(); channel++)
		{
			float X = 0.0;

			// Get audio for this channel
			const float *samples = audio->getReadPointer(channel);

			for (int sample = 0; sample < GetAudioSamplesCount(); sample++, X++)
			{
				// Sample value (scaled to -100 to 100)
				float value = samples[sample] * 100.0;

				// Set threshold near zero (so we don't allow near-zero values)
				// This prevents empty gaps from appearing in the waveform
				if (value > -zero_height && value < 0.0) {
				    value = -zero_height;
				} else if (value > 0.0 && value < zero_height) {
                    value = zero_height;
				}

				// Append a line segment for each sample
                lines.push_back(QPointF(X, Y));
                lines.push_back(QPointF(X, Y - value));
			}

			// Add Channel Label Coordinate
			labels.push_back(QPointF(5.0, Y - 5.0));

			// Increment Y
			Y += (200 + height_padding);
			total_width = X;
		}

		// Create blank image
		wave_image = std::make_shared<QImage>(
			total_width, total_height, QImage::Format_RGBA8888_Premultiplied);
		wave_image->fill(QColor(0,0,0,0));

		// Load QPainter with wave_image device
		QPainter painter(wave_image.get());

		// Set pen color
        QPen pen;
        pen.setColor(QColor(Red, Green, Blue, Alpha));
        pen.setWidthF(1.0);
        pen.setStyle(Qt::SolidLine);
        painter.setPen(pen);

		// Draw the waveform
		painter.drawLines(lines);
		painter.end();
	}
	else
	{
		// No audio samples present
		wave_image = std::make_shared<QImage>(width, height, QImage::Format_RGBA8888_Premultiplied);
		wave_image->fill(QColor(QString::fromStdString("#000000")));
	}

    // Resize Image (if needed)
    if (wave_image->width() != width || wave_image->height() != height) {
        QImage scaled_wave_image = wave_image->scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        wave_image = std::make_shared<QImage>(scaled_wave_image);
    }

	// Return new image
	return wave_image;
}

// Clear the waveform image (and deallocate its memory)
void Frame::ClearWaveform()
{
	if (wave_image)
		wave_image.reset();
}

// Get an audio waveform image pixels
const unsigned char* Frame::GetWaveformPixels(int width, int height, int Red, int Green, int Blue, int Alpha)
{
	// Get audio wave form image
	wave_image = GetWaveform(width, height, Red, Green, Blue, Alpha);

	// Return array of pixel packets
	return wave_image->constBits();
}

// Display the wave form
void Frame::DisplayWaveform()
{
	// Get audio wave form image
	GetWaveform(720, 480, 0, 123, 255, 255);

	if (!QApplication::instance()) {
		// Only create the QApplication once
		static int argc = 1;
		static char* argv[1] = {NULL};
		previewApp = std::make_shared<QApplication>(argc, argv);
	}

	// Create window
	QWidget previewWindow;
	previewWindow.setStyleSheet("background-color: #000000;");
	QHBoxLayout layout;

	// Create label with current frame's waveform image
	QLabel previewLabel;
	previewLabel.setPixmap(QPixmap::fromImage(*wave_image));
	previewLabel.setMask(QPixmap::fromImage(*wave_image).mask());
	layout.addWidget(&previewLabel);

	// Show the window
	previewWindow.setLayout(&layout);
	previewWindow.show();
	previewApp->exec();

	// Deallocate waveform image
	ClearWaveform();
}

// Get magnitude of range of samples (if channel is -1, return average of all channels for that sample)
float Frame::GetAudioSample(int channel, int sample, int magnitude_range)
{
	if (channel > 0) {
		// return average magnitude for a specific channel/sample range
		return audio->getMagnitude(channel, sample, magnitude_range);

	} else {
		// Return average magnitude for all channels
		return audio->getMagnitude(sample, magnitude_range);
	}
}

// Get an array of sample data
float* Frame::GetAudioSamples(int channel)
{
	// return JUCE audio data for this channel
	return audio->getWritePointer(channel);
}

// Get a planar array of sample data, using any sample rate
float* Frame::GetPlanarAudioSamples(int new_sample_rate, AudioResampler* resampler, int* sample_count)
{
	float *output = NULL;
	juce::AudioBuffer<float> *buffer(audio.get());
	int num_of_channels = audio->getNumChannels();
	int num_of_samples = GetAudioSamplesCount();

	// Resample to new sample rate (if needed)
	if (new_sample_rate != sample_rate)
	{
		// YES, RESAMPLE AUDIO
		resampler->SetBuffer(audio.get(), sample_rate, new_sample_rate);

		// Resample data, and return new buffer pointer
		buffer = resampler->GetResampledBuffer();

		// Update num_of_samples
		num_of_samples = buffer->getNumSamples();
	}

	// INTERLEAVE all samples together (channel 1 + channel 2 + channel 1 + channel 2, etc...)
	output = new float[num_of_channels * num_of_samples];
	int position = 0;

	// Loop through samples in each channel (combining them)
	for (int channel = 0; channel < num_of_channels; channel++)
	{
		for (int sample = 0; sample < num_of_samples; sample++)
		{
			// Add sample to output array
			output[position] = buffer->getReadPointer(channel)[sample];

			// increment position
			position++;
		}
	}

	// Update sample count (since it might have changed due to resampling)
	*sample_count = num_of_samples;

	// return combined array
	return output;
}


// Get an array of sample data (all channels interleaved together), using any sample rate
float* Frame::GetInterleavedAudioSamples(int new_sample_rate, AudioResampler* resampler, int* sample_count)
{
	float *output = NULL;
	juce::AudioBuffer<float> *buffer(audio.get());
	int num_of_channels = audio->getNumChannels();
	int num_of_samples = GetAudioSamplesCount();

	// Resample to new sample rate (if needed)
	if (new_sample_rate != sample_rate && resampler)
	{
		// YES, RESAMPLE AUDIO
		resampler->SetBuffer(audio.get(), sample_rate, new_sample_rate);

		// Resample data, and return new buffer pointer
		buffer = resampler->GetResampledBuffer();

		// Update num_of_samples
		num_of_samples = buffer->getNumSamples();
	}

	// INTERLEAVE all samples together (channel 1 + channel 2 + channel 1 + channel 2, etc...)
	output = new float[num_of_channels * num_of_samples];
	int position = 0;

	// Loop through samples in each channel (combining them)
	for (int sample = 0; sample < num_of_samples; sample++)
	{
		for (int channel = 0; channel < num_of_channels; channel++)
		{
			// Add sample to output array
			output[position] = buffer->getReadPointer(channel)[sample];

			// increment position
			position++;
		}
	}

	// Update sample count (since it might have changed due to resampling)
	*sample_count = num_of_samples;

	// return combined array
	return output;
}

// Get number of audio channels
int Frame::GetAudioChannelsCount()
{
    const std::lock_guard<std::recursive_mutex> lock(addingAudioMutex);
	if (audio)
		return audio->getNumChannels();
	else
		return 0;
}

// Get number of audio samples
int Frame::GetAudioSamplesCount()
{
    const std::lock_guard<std::recursive_mutex> lock(addingAudioMutex);
	return max_audio_sample;
}

juce::AudioBuffer<float> *Frame::GetAudioSampleBuffer()
{
    return audio.get();
}

// Get the size in bytes of this frame (rough estimate)
int64_t Frame::GetBytes()
{
	int64_t total_bytes = 0;
	if (image) {
		total_bytes += static_cast<int64_t>(
		    width * height * sizeof(char) * 4);
	}
	if (audio) {
		// approximate audio size (sample rate / 24 fps)
		total_bytes += (sample_rate / 24.0) * sizeof(float);
	}

	// return size of this frame
	return total_bytes;
}

// Get pixel data (as packets)
const unsigned char* Frame::GetPixels()
{
	// Check for blank image
	if (!image)
		// Fill with black
		AddColor(width, height, color);

	// Return array of pixel packets
	return image->constBits();
}

// Get pixel data (for only a single scan-line)
const unsigned char* Frame::GetPixels(int row)
{
	// Check for blank image
	if (!image)
		// Fill with black
		AddColor(width, height, color);

	// Return array of pixel packets
	return image->constScanLine(row);
}

// Check a specific pixel color value (returns True/False)
bool Frame::CheckPixel(int row, int col, int red, int green, int blue, int alpha, int threshold) {
	int col_pos = col * 4; // Find column array position
	if (!image || row < 0 || row >= (height - 1) ||
		col_pos < 0 || col_pos >= (width - 1) ) {
		// invalid row / col
		return false;
	}
	// Check pixel color
	const unsigned char* pixels = GetPixels(row);
	if (pixels[col_pos + 0] >= (red - threshold) && pixels[col_pos + 0] <= (red + threshold) &&
		pixels[col_pos + 1] >= (green - threshold) && pixels[col_pos + 1] <= (green + threshold) &&
		pixels[col_pos + 2] >= (blue - threshold) && pixels[col_pos + 2] <= (blue + threshold) &&
		pixels[col_pos + 3] >= (alpha - threshold) && pixels[col_pos + 3] <= (alpha + threshold)) {
		// Pixel color matches successfully
		return true;
	} else {
		// Pixel color does not match
		return false;
	}
}

// Set Pixel Aspect Ratio
void Frame::SetPixelRatio(int num, int den)
{
	pixel_ratio.num = num;
	pixel_ratio.den = den;
}

// Set frame number
void Frame::SetFrameNumber(int64_t new_number)
{
	number = new_number;
}

// Calculate the # of samples per video frame (for a specific frame number and frame rate)
int Frame::GetSamplesPerFrame(int64_t number, Fraction fps, int sample_rate, int channels)
{
	// Get the total # of samples for the previous frame, and the current frame (rounded)
	double fps_rate = fps.Reciprocal().ToDouble();

	// Determine previous samples total, and make sure it's evenly divisible by the # of channels
	double previous_samples = (sample_rate * fps_rate) * (number - 1);
	double previous_samples_remainder = fmod(previous_samples, (double)channels); // subtract the remainder to the total (to make it evenly divisible)
	previous_samples -= previous_samples_remainder;

	// Determine the current samples total, and make sure it's evenly divisible by the # of channels
	double total_samples = (sample_rate * fps_rate) * number;
	double total_samples_remainder = fmod(total_samples, (double)channels); // subtract the remainder to the total (to make it evenly divisible)
	total_samples -= total_samples_remainder;

	// Subtract the previous frame's total samples with this frame's total samples.  Not all sample rates can
	// be evenly divided into frames, so each frame can have have different # of samples.
	int samples_per_frame = round(total_samples - previous_samples);
	if (samples_per_frame < 0)
		samples_per_frame = 0;
	return samples_per_frame;
}

// Calculate the # of samples per video frame (for the current frame number)
int Frame::GetSamplesPerFrame(Fraction fps, int sample_rate, int channels)
{
	return GetSamplesPerFrame(number, fps, sample_rate, channels);
}

// Get height of image
int Frame::GetHeight()
{
	return height;
}

// Get height of image
int Frame::GetWidth()
{
	return width;
}

// Get the original sample rate of this frame's audio data
int Frame::SampleRate()
{
	return sample_rate;
}

// Get the original sample rate of this frame's audio data
ChannelLayout Frame::ChannelsLayout()
{
	return channel_layout;
}


// Save the frame image to the specified path.  The image format is determined from the extension (i.e. image.PNG, image.JPEG)
void Frame::Save(std::string path, float scale, std::string format, int quality)
{
	// Get preview image
	std::shared_ptr<QImage> previewImage = GetImage();

    // Update the image to reflect the correct pixel aspect ration (i.e. to fix non-square pixels)
    if (pixel_ratio.num != 1 || pixel_ratio.den != 1)
    {
        // Resize to fix DAR
        previewImage = std::make_shared<QImage>(previewImage->scaled(
                previewImage->size().width(), previewImage->size().height() * pixel_ratio.Reciprocal().ToDouble(),
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

	// scale image if needed
	if (fabs(scale) > 1.001 || fabs(scale) < 0.999)
	{
		// Resize image
		previewImage = std::make_shared<QImage>(previewImage->scaled(
		        previewImage->size().width() * scale, previewImage->size().height() * scale,
		        Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}

	// Save image
	previewImage->save(QString::fromStdString(path), format.c_str(), quality);
}

// Thumbnail the frame image to the specified path.  The image format is determined from the extension (i.e. image.PNG, image.JPEG)
void Frame::Thumbnail(std::string path, int new_width, int new_height, std::string mask_path, std::string overlay_path,
		std::string background_color, bool ignore_aspect, std::string format, int quality, float rotate) {

	// Create blank thumbnail image & fill background color
	auto thumbnail = std::make_shared<QImage>(
		new_width, new_height, QImage::Format_RGBA8888_Premultiplied);
	thumbnail->fill(QColor(QString::fromStdString(background_color)));

	// Create painter
	QPainter painter(thumbnail.get());
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);

	// Get preview image
	std::shared_ptr<QImage> previewImage = GetImage();

	// Update the image to reflect the correct pixel aspect ration (i.e. to fix non-squar pixels)
	if (pixel_ratio.num != 1 || pixel_ratio.den != 1)
	{
		// Calculate correct DAR (display aspect ratio)
		int aspect_width = previewImage->size().width();
		int aspect_height = previewImage->size().height() * pixel_ratio.Reciprocal().ToDouble();

		// Resize to fix DAR
		previewImage = std::make_shared<QImage>(previewImage->scaled(
			aspect_width, aspect_height,
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	}

	// Resize frame image
	if (ignore_aspect)
		// Ignore aspect ratio
		previewImage = std::make_shared<QImage>(previewImage->scaled(
			new_width, new_height,
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
	else
		// Maintain aspect ratio
		previewImage = std::make_shared<QImage>(previewImage->scaled(
			new_width, new_height,
			Qt::KeepAspectRatio, Qt::SmoothTransformation));

	// Composite frame image onto background (centered)
	int x = (new_width - previewImage->size().width()) / 2.0; // center
	int y = (new_height - previewImage->size().height()) / 2.0; // center
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);


	// Create transform and rotate (if needed)
	QTransform transform;
	float origin_x = previewImage->width() / 2.0;
	float origin_y = previewImage->height() / 2.0;
	transform.translate(origin_x, origin_y);
	transform.rotate(rotate);
	transform.translate(-origin_x,-origin_y);
	painter.setTransform(transform);

	// Draw image onto QImage
	painter.drawImage(x, y, *previewImage);


	// Overlay Image (if any)
	if (overlay_path != "") {
		// Open overlay
		auto overlay = std::make_shared<QImage>();
		overlay->load(QString::fromStdString(overlay_path));

		// Set pixel format
		overlay = std::make_shared<QImage>(
			overlay->convertToFormat(QImage::Format_RGBA8888_Premultiplied));

		// Resize to fit
		overlay = std::make_shared<QImage>(overlay->scaled(
			new_width, new_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

		// Composite onto thumbnail
		painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
		painter.drawImage(0, 0, *overlay);
	}


	// Mask Image (if any)
	if (mask_path != "") {
		// Open mask
		auto mask = std::make_shared<QImage>();
		mask->load(QString::fromStdString(mask_path));

		// Set pixel format
		mask = std::make_shared<QImage>(
			mask->convertToFormat(QImage::Format_RGBA8888_Premultiplied));

		// Resize to fit
		mask = std::make_shared<QImage>(mask->scaled(
			new_width, new_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

		// Negate mask
		mask->invertPixels();

		// Get pixels
		unsigned char *pixels = (unsigned char *) thumbnail->bits();
		const unsigned char *mask_pixels = (const unsigned char *) mask->constBits();

		// Convert the mask image to grayscale
		// Loop through pixels
		for (int pixel = 0, byte_index=0; pixel < new_width * new_height; pixel++, byte_index+=4)
		{
			// Get the RGB values from the pixel
			int gray_value = qGray(mask_pixels[byte_index], mask_pixels[byte_index] + 1, mask_pixels[byte_index] + 2);
			int Frame_Alpha = pixels[byte_index + 3];
			int Mask_Value = constrain(Frame_Alpha - gray_value);

			// Set all alpha pixels to gray value
			pixels[byte_index + 3] = Mask_Value;
		}
	}


	// End painter
	painter.end();

	// Save image
	thumbnail->save(QString::fromStdString(path), format.c_str(), quality);
}

// Constrain a color value from 0 to 255
int Frame::constrain(int color_value)
{
	// Constrain new color from 0 to 255
	if (color_value < 0)
		color_value = 0;
	else if (color_value > 255)
		color_value = 255;

	return color_value;
}

void Frame::AddColor(int new_width, int new_height, std::string new_color)
{
     const std::lock_guard<std::recursive_mutex> lock(addingImageMutex);
     // Update parameters
    width = new_width;
    height = new_height;
    color = new_color;
    AddColor(QColor(QString::fromStdString(new_color)));
}

// Add (or replace) pixel data to the frame (based on a solid color)
void Frame::AddColor(const QColor& new_color)
{
	// Create new image object, and fill with pixel data
	const std::lock_guard<std::recursive_mutex> lock(addingImageMutex);
	image = std::make_shared<QImage>(width, height, QImage::Format_RGBA8888_Premultiplied);

	// Fill with solid color
	image->fill(new_color);
	has_image_data = true;
}

// Add (or replace) pixel data to the frame
void Frame::AddImage(
	int new_width, int new_height, int bytes_per_pixel,
	QImage::Format type, const unsigned char *pixels_)
{

  // Create new image object from pixel data
	auto new_image = std::make_shared<QImage>(
		pixels_,
		new_width, new_height,
		new_width * bytes_per_pixel,
		type,
		(QImageCleanupFunction) &openshot::cleanUpBuffer,
		(void*) pixels_
	);
	AddImage(new_image);
}

// Add (or replace) pixel data to the frame
void Frame::AddImage(std::shared_ptr<QImage> new_image)
{
	// Ignore blank images
	if (!new_image)
		return;

	// assign image data
	const std::lock_guard<std::recursive_mutex> lock(addingImageMutex);
	image = new_image;

	// Always convert to Format_RGBA8888_Premultiplied (if different)
	if (image->format() != QImage::Format_RGBA8888_Premultiplied)
		*image = image->convertToFormat(QImage::Format_RGBA8888_Premultiplied);

	// Update height and width
	width = image->width();
	height = image->height();
	has_image_data = true;
}

// Add (or replace) pixel data to the frame (for only the odd or even lines)
void Frame::AddImage(std::shared_ptr<QImage> new_image, bool only_odd_lines)
{
	// Ignore blank new_image
	if (!new_image)
		return;

	// Check for blank source image
	if (!image) {
		// Replace the blank source image
		AddImage(new_image);

	} else {
		// Ignore image of different sizes or formats
		bool ret=false;
		if (image == new_image || image->size() != new_image->size()) {
			ret = true;
		}
		else if (new_image->format() != QImage::Format_RGBA8888_Premultiplied) {
			new_image = std::make_shared<QImage>(
					new_image->convertToFormat(QImage::Format_RGBA8888_Premultiplied));
		}
		if (ret) {
			return;
		}

		// Get the frame's image
		const std::lock_guard<std::recursive_mutex> lock(addingImageMutex);
		unsigned char *pixels = image->bits();
		const unsigned char *new_pixels = new_image->constBits();

		// Loop through the scanlines of the image (even or odd)
		int start = 0;
		if (only_odd_lines)
			start = 1;

		for (int row = start; row < image->height(); row += 2) {
			int offset = row * image->bytesPerLine();
			memcpy(pixels + offset, new_pixels + offset, image->bytesPerLine());
		}

		// Update height and width
		height = image->height();
		width = image->width();
		has_image_data = true;
	}
}


// Resize audio container to hold more (or less) samples and channels
void Frame::ResizeAudio(int channels, int length, int rate, ChannelLayout layout)
{
    const std::lock_guard<std::recursive_mutex> lock(addingAudioMutex);

    // Resize JUCE audio buffer
	audio->setSize(channels, length, true, true, false);
	channel_layout = layout;
	sample_rate = rate;

	// Calculate max audio sample added
	max_audio_sample = length;
}

// Add audio samples to a specific channel
void Frame::AddAudio(bool replaceSamples, int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource = 1.0f) {
	const std::lock_guard<std::recursive_mutex> lock(addingAudioMutex);

	// Clamp starting sample to 0
	int destStartSampleAdjusted = max(destStartSample, 0);

	// Extend audio container to hold more (or less) samples and channels.. if needed
	int new_length = destStartSampleAdjusted + numSamples;
	int new_channel_length = audio->getNumChannels();
	if (destChannel >= new_channel_length)
		new_channel_length = destChannel + 1;
	if (new_length > audio->getNumSamples() || new_channel_length > audio->getNumChannels())
		audio->setSize(new_channel_length, new_length, true, true, false);

	// Clear the range of samples first (if needed)
	if (replaceSamples)
		audio->clear(destChannel, destStartSampleAdjusted, numSamples);

	// Add samples to frame's audio buffer
	audio->addFrom(destChannel, destStartSampleAdjusted, source, numSamples, gainToApplyToSource);
	has_audio_data = true;

	// Calculate max audio sample added
	if (new_length > max_audio_sample)
		max_audio_sample = new_length;
}

// Apply gain ramp (i.e. fading volume)
void Frame::ApplyGainRamp(int destChannel, int destStartSample, int numSamples, float initial_gain = 0.0f, float final_gain = 1.0f)
{
    const std::lock_guard<std::recursive_mutex> lock(addingAudioMutex);

    // Apply gain ramp
	audio->applyGainRamp(destChannel, destStartSample, numSamples, initial_gain, final_gain);
}

// Get pointer to Magick++ image object
std::shared_ptr<QImage> Frame::GetImage()
{
	// Check for blank image
	if (!image)
		// Fill with black
		AddColor(width, height, color);

    return image;
}

#ifdef USE_OPENCV

// Convert Qimage to Mat
cv::Mat Frame::Qimage2mat( std::shared_ptr<QImage>& qimage) {

    cv::Mat mat = cv::Mat(qimage->height(), qimage->width(), CV_8UC4, (uchar*)qimage->constBits(), qimage->bytesPerLine()).clone();
    cv::Mat mat2 = cv::Mat(mat.rows, mat.cols, CV_8UC3 );
    int from_to[] = { 0,0,  1,1,  2,2 };
    cv::mixChannels( &mat, 1, &mat2, 1, from_to, 3 );
	cv::cvtColor(mat2, mat2, cv::COLOR_RGB2BGR);
    return mat2;
}

// Get pointer to OpenCV image object
cv::Mat Frame::GetImageCV()
{
	// Check for blank image
	if (!image)
		// Fill with black
		AddColor(width, height, color);

	// if (imagecv.empty())
	// Convert Qimage to Mat
	imagecv = Qimage2mat(image);

	return imagecv;
}

std::shared_ptr<QImage> Frame::Mat2Qimage(cv::Mat img){
	cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
	QImage qimg((uchar*) img.data, img.cols, img.rows, img.step, QImage::Format_RGB888);

	std::shared_ptr<QImage> imgIn = std::make_shared<QImage>(qimg.copy());

	// Always convert to RGBA8888 (if different)
	if (imgIn->format() != QImage::Format_RGBA8888_Premultiplied)
		*imgIn = imgIn->convertToFormat(QImage::Format_RGBA8888_Premultiplied);

	return imgIn;
}

// Set pointer to OpenCV image object
void Frame::SetImageCV(cv::Mat _image)
{
	imagecv = _image;
	image = Mat2Qimage(_image);
}
#endif

// Play audio samples for this frame
void Frame::Play()
{
	// Check if samples are present
	if (!GetAudioSamplesCount())
		return;

	juce::AudioDeviceManager deviceManager;
	juce::String error = deviceManager.initialise (
	        0, /* number of input channels */
	        2, /* number of output channels */
	        0, /* no XML settings.. */
	        true  /* select default device on failure */);

	// Output error (if any)
	if (error.isNotEmpty()) {
		cout << "Error on initialise(): " << error << endl;
	}

	juce::AudioSourcePlayer audioSourcePlayer;
	deviceManager.addAudioCallback (&audioSourcePlayer);

	std::unique_ptr<AudioBufferSource> my_source;
	my_source.reset (new AudioBufferSource (audio.get()));

	// Create TimeSliceThread for audio buffering
	juce::TimeSliceThread my_thread("Audio buffer thread");

	// Start thread
	my_thread.startThread();

	juce::AudioTransportSource transport1;
	transport1.setSource (my_source.get(),
			5000, // tells it to buffer this many samples ahead
			&my_thread,
			(double) sample_rate,
			audio->getNumChannels()); // sample rate of source
	transport1.setPosition (0);
	transport1.setGain(1.0);


	// Create MIXER
	juce::MixerAudioSource mixer;
	mixer.addInputSource(&transport1, false);
	audioSourcePlayer.setSource (&mixer);

	// Start transports
	transport1.start();

	while (transport1.isPlaying())
	{
		cout << "playing" << endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	cout << "DONE!!!" << endl;

	transport1.stop();
    transport1.setSource (0);
    audioSourcePlayer.setSource (0);
    my_thread.stopThread(500);
    deviceManager.removeAudioCallback (&audioSourcePlayer);
    deviceManager.closeAudioDevice();
    deviceManager.removeAllChangeListeners();
    deviceManager.dispatchPendingMessages();

	cout << "End of Play()" << endl;


}

// Add audio silence
void Frame::AddAudioSilence(int numSamples)
{
    const std::lock_guard<std::recursive_mutex> lock(addingAudioMutex);

    // Resize audio container
	audio->setSize(channels, numSamples, false, true, false);
	audio->clear();
	has_audio_data = true;

	// Calculate max audio sample added
	if (numSamples > max_audio_sample)
		max_audio_sample = numSamples;
}
