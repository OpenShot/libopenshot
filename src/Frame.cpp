/**
 * @file
 * @brief Source file for Frame class
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

#include "../include/Frame.h"

using namespace std;
using namespace openshot;

// Constructor - blank frame (300x200 blank image, 48kHz audio silence)
Frame::Frame() : number(1), pixel_ratio(1,1), channels(2), width(1), height(1), color("#000000"),
		channel_layout(LAYOUT_STEREO), sample_rate(44100), qbuffer(NULL), has_audio_data(false), has_image_data(false),
		max_audio_sample(0)
{
	// Init the image magic and audio buffer
	audio = std::shared_ptr<juce::AudioSampleBuffer>(new juce::AudioSampleBuffer(channels, 0));

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Constructor - image only (48kHz audio silence)
Frame::Frame(int64_t number, int width, int height, string color)
	: number(number), pixel_ratio(1,1), channels(2), width(width), height(height), color(color),
	  channel_layout(LAYOUT_STEREO), sample_rate(44100), qbuffer(NULL), has_audio_data(false), has_image_data(false),
	  max_audio_sample(0)
{
	// Init the image magic and audio buffer
	audio = std::shared_ptr<juce::AudioSampleBuffer>(new juce::AudioSampleBuffer(channels, 0));

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Constructor - audio only (300x200 blank image)
Frame::Frame(int64_t number, int samples, int channels) :
		number(number), pixel_ratio(1,1), channels(channels), width(1), height(1), color("#000000"),
		channel_layout(LAYOUT_STEREO), sample_rate(44100), qbuffer(NULL), has_audio_data(false), has_image_data(false),
		max_audio_sample(0)
{
	// Init the image magic and audio buffer
	audio = std::shared_ptr<juce::AudioSampleBuffer>(new juce::AudioSampleBuffer(channels, samples));

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Constructor - image & audio
Frame::Frame(int64_t number, int width, int height, string color, int samples, int channels)
	: number(number), pixel_ratio(1,1), channels(channels), width(width), height(height), color(color),
	  channel_layout(LAYOUT_STEREO), sample_rate(44100), qbuffer(NULL), has_audio_data(false), has_image_data(false),
	  max_audio_sample(0)
{
	// Init the image magic and audio buffer
	audio = std::shared_ptr<juce::AudioSampleBuffer>(new juce::AudioSampleBuffer(channels, samples));

	// initialize the audio samples to zero (silence)
	audio->clear();
};


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
	has_audio_data = other.has_image_data;
	has_image_data = other.has_image_data;
	sample_rate = other.sample_rate;
	pixel_ratio = Fraction(other.pixel_ratio.num, other.pixel_ratio.den);
	color = other.color;

	if (other.image)
		image = std::shared_ptr<QImage>(new QImage(*(other.image)));
	if (other.audio)
		audio = std::shared_ptr<juce::AudioSampleBuffer>(new juce::AudioSampleBuffer(*(other.audio)));
	if (other.wave_image)
		wave_image = std::shared_ptr<QImage>(new QImage(*(other.wave_image)));
}

// Descructor
Frame::~Frame() {
	// Clear all pointers
	image.reset();
	audio.reset();
}

// Display the frame image to the screen (primarily used for debugging reasons)
void Frame::Display()
{
	if (!QApplication::instance()) {
		// Only create the QApplication once
		static int argc = 1;
		static char* argv[1] = {NULL};
		previewApp = std::shared_ptr<QApplication>(new QApplication(argc, argv));
	}

	// Get preview image
	std::shared_ptr<QImage> previewImage = GetImage();

	// Update the image to reflect the correct pixel aspect ration (i.e. to fix non-squar pixels)
	if (pixel_ratio.num != 1 || pixel_ratio.den != 1)
	{
		// Calculate correct DAR (display aspect ratio)
		int new_width = previewImage->size().width();
		int new_height = previewImage->size().height() * pixel_ratio.Reciprocal().ToDouble();

		// Resize to fix DAR
		previewImage = std::shared_ptr<QImage>(new QImage(previewImage->scaled(new_width, new_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
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

		// Loop through each audio channel
		int Y = 100;
		for (int channel = 0; channel < audio->getNumChannels(); channel++)
		{
			int X = 0;

			// Get audio for this channel
			const float *samples = audio->getReadPointer(channel);

			for (int sample = 0; sample < GetAudioSamplesCount(); sample++, X++)
			{
				// Sample value (scaled to -100 to 100)
				float value = samples[sample] * 100;

				// Append a line segment for each sample
				if (value != 0.0) {
					// LINE
					lines.push_back(QPointF(X,Y));
					lines.push_back(QPointF(X,Y-value));
				}
				else {
					// DOT
					lines.push_back(QPointF(X,Y));
					lines.push_back(QPointF(X,Y));
				}
			}

			// Add Channel Label Coordinate
			labels.push_back(QPointF(5, Y - 5));

			// Increment Y
			Y += (200 + height_padding);
			total_width = X;
		}

		// Create blank image
		wave_image = std::shared_ptr<QImage>(new QImage(total_width, total_height, QImage::Format_RGBA8888));
		wave_image->fill(QColor(0,0,0,0));

		// Load QPainter with wave_image device
		QPainter painter(wave_image.get());

		// Set pen color
		painter.setPen(QColor(Red, Green, Blue, Alpha));

		// Draw the waveform
		painter.drawLines(lines);
		painter.end();

		// Loop through the channels labels (and draw the text)
		// TODO: Configure Fonts in Qt5 correctly, so the drawText method does not crash
//		painter.setFont(QFont(QString("Arial"), 16, 1, false));
//		for (int channel = 0; channel < labels.size(); channel++) {
//			stringstream label;
//			label << "Channel " << channel;
//		    painter.drawText(labels.at(channel), QString::fromStdString(label.str()));
//		}

		// Resize Image (if requested)
		if (width != total_width || height != total_height) {
			QImage scaled_wave_image = wave_image->scaled(width, height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
			wave_image = std::shared_ptr<QImage>(new QImage(scaled_wave_image));
		}
	}
	else
	{
		// No audio samples present
		wave_image = std::shared_ptr<QImage>(new QImage(width, height, QImage::Format_RGBA8888));
		wave_image->fill(QColor(QString::fromStdString("#000000")));
	}

	// Return new image
	return wave_image;
}

// Clear the waveform image (and deallocate it's memory)
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
	return wave_image->bits();
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
		previewApp = std::shared_ptr<QApplication>(new QApplication(argc, argv));
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
	AudioSampleBuffer *buffer(audio.get());
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
	AudioSampleBuffer *buffer(audio.get());
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
    const GenericScopedLock<CriticalSection> lock(addingAudioSection);
	if (audio)
		return audio->getNumChannels();
	else
		return 0;
}

// Get number of audio samples
int Frame::GetAudioSamplesCount()
{
    const GenericScopedLock<CriticalSection> lock(addingAudioSection);
	return max_audio_sample;
}

juce::AudioSampleBuffer *Frame::GetAudioSampleBuffer()
{
    return audio.get();
}

// Get the size in bytes of this frame (rough estimate)
int64_t Frame::GetBytes()
{
	int64_t total_bytes = 0;
	if (image)
		total_bytes += (width * height * sizeof(char) * 4);
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
	return image->bits();
}

// Get pixel data (for only a single scan-line)
const unsigned char* Frame::GetPixels(int row)
{
	// Return array of pixel packets
	return image->scanLine(row);
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
void Frame::Save(string path, float scale, string format, int quality)
{
	// Get preview image
	std::shared_ptr<QImage> previewImage = GetImage();

	// scale image if needed
	if (abs(scale) > 1.001 || abs(scale) < 0.999)
	{
		int new_width = width;
		int new_height = height;

		// Update the image to reflect the correct pixel aspect ration (i.e. to fix non-squar pixels)
		if (pixel_ratio.num != 1 || pixel_ratio.den != 1)
		{
			// Calculate correct DAR (display aspect ratio)
			int new_width = previewImage->size().width();
			int new_height = previewImage->size().height() * pixel_ratio.Reciprocal().ToDouble();

			// Resize to fix DAR
			previewImage = std::shared_ptr<QImage>(new QImage(previewImage->scaled(new_width, new_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
		}

		// Resize image
		previewImage = std::shared_ptr<QImage>(new QImage(previewImage->scaled(new_width * scale, new_height * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
	}

	// Save image
	previewImage->save(QString::fromStdString(path), format.c_str(), quality);
}

// Thumbnail the frame image to the specified path.  The image format is determined from the extension (i.e. image.PNG, image.JPEG)
void Frame::Thumbnail(string path, int new_width, int new_height, string mask_path, string overlay_path,
		string background_color, bool ignore_aspect, string format, int quality, float rotate) {

	// Create blank thumbnail image & fill background color
	std::shared_ptr<QImage> thumbnail = std::shared_ptr<QImage>(new QImage(new_width, new_height, QImage::Format_RGBA8888));
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
		previewImage = std::shared_ptr<QImage>(new QImage(previewImage->scaled(aspect_width, aspect_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
	}

	// Resize frame image
	if (ignore_aspect)
		// Ignore aspect ratio
		previewImage = std::shared_ptr<QImage>(new QImage(previewImage->scaled(new_width, new_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
	else
		// Maintain aspect ratio
		previewImage = std::shared_ptr<QImage>(new QImage(previewImage->scaled(new_width, new_height, Qt::KeepAspectRatio, Qt::SmoothTransformation)));

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
		std::shared_ptr<QImage> overlay = std::shared_ptr<QImage>(new QImage());
		overlay->load(QString::fromStdString(overlay_path));

		// Set pixel format
		overlay = std::shared_ptr<QImage>(new QImage(overlay->convertToFormat(QImage::Format_RGBA8888)));

		// Resize to fit
		overlay = std::shared_ptr<QImage>(new QImage(overlay->scaled(new_width, new_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));

		// Composite onto thumbnail
		painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
		painter.drawImage(0, 0, *overlay);
	}


	// Mask Image (if any)
	if (mask_path != "") {
		// Open mask
		std::shared_ptr<QImage> mask = std::shared_ptr<QImage>(new QImage());
		mask->load(QString::fromStdString(mask_path));

		// Set pixel format
		mask = std::shared_ptr<QImage>(new QImage(mask->convertToFormat(QImage::Format_RGBA8888)));

		// Resize to fit
		mask = std::shared_ptr<QImage>(new QImage(mask->scaled(new_width, new_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));

		// Negate mask
		mask->invertPixels();

		// Get pixels
		unsigned char *pixels = (unsigned char *) thumbnail->bits();
		unsigned char *mask_pixels = (unsigned char *) mask->bits();

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

// Add (or replace) pixel data to the frame (based on a solid color)
void Frame::AddColor(int new_width, int new_height, string new_color)
{
	// Set color
	color = new_color;

	// Create new image object, and fill with pixel data
	const GenericScopedLock<CriticalSection> lock(addingImageSection);
	#pragma omp critical (AddImage)
	{
		image = std::shared_ptr<QImage>(new QImage(new_width, new_height, QImage::Format_RGBA8888));

		// Fill with solid color
		image->fill(QColor(QString::fromStdString(color)));
	}
	// Update height and width
	width = image->width();
	height = image->height();
	has_image_data = true;
}

// Add (or replace) pixel data to the frame
void Frame::AddImage(int new_width, int new_height, int bytes_per_pixel, QImage::Format type, const unsigned char *pixels_)
{
	// Create new buffer
	const GenericScopedLock<CriticalSection> lock(addingImageSection);
	int buffer_size = new_width * new_height * bytes_per_pixel;
	qbuffer = new unsigned char[buffer_size]();

	// Copy buffer data
	memcpy((unsigned char*)qbuffer, pixels_, buffer_size);

	// Create new image object, and fill with pixel data
	#pragma omp critical (AddImage)
	{
		image = std::shared_ptr<QImage>(new QImage(qbuffer, new_width, new_height, new_width * bytes_per_pixel, type, (QImageCleanupFunction) &openshot::Frame::cleanUpBuffer, (void*) qbuffer));

		// Always convert to RGBA8888 (if different)
		if (image->format() != QImage::Format_RGBA8888)
			*image  = image->convertToFormat(QImage::Format_RGBA8888);

			// Update height and width
		width = image->width();
		height = image->height();
		has_image_data = true;
	}
}

// Add (or replace) pixel data to the frame
void Frame::AddImage(std::shared_ptr<QImage> new_image)
{
	// Ignore blank images
	if (!new_image)
		return;

	// assign image data
	const GenericScopedLock<CriticalSection> lock(addingImageSection);
	#pragma omp critical (AddImage)
	{
		image = new_image;

		// Always convert to RGBA8888 (if different)
		if (image->format() != QImage::Format_RGBA8888)
			*image = image->convertToFormat(QImage::Format_RGBA8888);

		// Update height and width
		width = image->width();
		height = image->height();
		has_image_data = true;
	}
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
		#pragma omp critical (AddImage)
		if (image == new_image || image->size() != image->size() || image->format() != image->format())
			ret=true;
		if (ret)
			return;

		// Get the frame's image
		const GenericScopedLock<CriticalSection> lock(addingImageSection);
		#pragma omp critical (AddImage)
		{
			const unsigned char *pixels = image->bits();
			const unsigned char *new_pixels = new_image->bits();

			// Loop through the scanlines of the image (even or odd)
			int start = 0;
			if (only_odd_lines)
				start = 1;
				for (int row = start; row < image->height(); row += 2) {
					memcpy((unsigned char *) pixels, new_pixels + (row * image->bytesPerLine()), image->bytesPerLine());
					new_pixels += image->bytesPerLine();
				}

			// Update height and width
			width = image->width();
			height = image->height();
			has_image_data = true;
		}
	}
}


// Resize audio container to hold more (or less) samples and channels
void Frame::ResizeAudio(int channels, int length, int rate, ChannelLayout layout)
{
    const GenericScopedLock<CriticalSection> lock(addingAudioSection);

    // Resize JUCE audio buffer
	audio->setSize(channels, length, true, true, false);
	channel_layout = layout;
	sample_rate = rate;

	// Calculate max audio sample added
	max_audio_sample = length;
}

// Add audio samples to a specific channel
void Frame::AddAudio(bool replaceSamples, int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource = 1.0f) {
	const GenericScopedLock<CriticalSection> lock(addingAudioSection);
	#pragma omp critical (adding_audio)
    {
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
}

// Apply gain ramp (i.e. fading volume)
void Frame::ApplyGainRamp(int destChannel, int destStartSample, int numSamples, float initial_gain = 0.0f, float final_gain = 1.0f)
{
    const GenericScopedLock<CriticalSection> lock(addingAudioSection);

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

#ifdef USE_IMAGEMAGICK
// Get pointer to ImageMagick image object
std::shared_ptr<Magick::Image> Frame::GetMagickImage()
{
	// Check for blank image
	if (!image)
		// Fill with black
		AddColor(width, height, "#000000");

	// Get the pixels from the frame image
	QRgb const *tmpBits = (const QRgb*)image->bits();

	// Create new image object, and fill with pixel data
	std::shared_ptr<Magick::Image> magick_image = std::shared_ptr<Magick::Image>(new Magick::Image(image->width(), image->height(),"RGBA", Magick::CharPixel, tmpBits));

	// Give image a transparent background color
	magick_image->backgroundColor(Magick::Color("none"));
	magick_image->virtualPixelMethod(Magick::TransparentVirtualPixelMethod);
	magick_image->matte(true);

	return magick_image;
}
#endif

#ifdef USE_IMAGEMAGICK
// Get pointer to QImage of frame
void Frame::AddMagickImage(std::shared_ptr<Magick::Image> new_image)
{
	const int BPP = 4;
	const std::size_t bufferSize = new_image->columns() * new_image->rows() * BPP;

	/// Use realloc for fast memory allocation.
	/// TODO: consider locking the buffer for mt safety
	//qbuffer = reinterpret_cast<unsigned char*>(realloc(qbuffer, bufferSize));
	qbuffer = new unsigned char[bufferSize]();
	unsigned char *buffer = (unsigned char*)qbuffer;

    // Iterate through the pixel packets, and load our own buffer
	// Each color needs to be scaled to 8 bit (using the ImageMagick built-in ScaleQuantumToChar function)
	int numcopied = 0;
    Magick::PixelPacket *pixels = new_image->getPixels(0,0, new_image->columns(), new_image->rows());
    for (int n = 0, i = 0; n < new_image->columns() * new_image->rows(); n += 1, i += 4) {
    	buffer[i+0] = MagickCore::ScaleQuantumToChar((Magick::Quantum) pixels[n].red);
    	buffer[i+1] = MagickCore::ScaleQuantumToChar((Magick::Quantum) pixels[n].green);
    	buffer[i+2] = MagickCore::ScaleQuantumToChar((Magick::Quantum) pixels[n].blue);
    	buffer[i+3] = 255 - MagickCore::ScaleQuantumToChar((Magick::Quantum) pixels[n].opacity);
    	numcopied+=4;
    }

    // Create QImage of frame data
    image = std::shared_ptr<QImage>(new QImage(qbuffer, width, height, width * BPP, QImage::Format_RGBA8888, (QImageCleanupFunction) &cleanUpBuffer, (void*) qbuffer));

	// Update height and width
	width = image->width();
	height = image->height();
	has_image_data = true;
}
#endif

// Play audio samples for this frame
void Frame::Play()
{
	// Check if samples are present
	if (!GetAudioSamplesCount())
		return;

	AudioDeviceManager deviceManager;
	deviceManager.initialise (0, /* number of input channels */
	        2, /* number of output channels */
	        0, /* no XML settings.. */
	        true  /* select default device on failure */);
	//deviceManager.playTestSound();

	AudioSourcePlayer audioSourcePlayer;
	deviceManager.addAudioCallback (&audioSourcePlayer);

	ScopedPointer<AudioBufferSource> my_source;
	my_source = new AudioBufferSource(audio.get());

	// Create TimeSliceThread for audio buffering
	TimeSliceThread my_thread("Audio buffer thread");

	// Start thread
	my_thread.startThread();

	AudioTransportSource transport1;
	transport1.setSource (my_source,
			5000, // tells it to buffer this many samples ahead
			&my_thread,
			(double) sample_rate,
			audio->getNumChannels()); // sample rate of source
	transport1.setPosition (0);
	transport1.setGain(1.0);


	// Create MIXER
	MixerAudioSource mixer;
	mixer.addInputSource(&transport1, false);
	audioSourcePlayer.setSource (&mixer);

	// Start transports
	transport1.start();

	while (transport1.isPlaying())
	{
		cout << "playing" << endl;
		usleep(1000000);
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

// Clean up buffer after QImage is deleted
void Frame::cleanUpBuffer(void *info)
{
	if (info)
	{
		// Remove buffer since QImage tells us to
		unsigned char* ptr_to_qbuffer = (unsigned char*) info;
		delete[] ptr_to_qbuffer;
	}
}

// Add audio silence
void Frame::AddAudioSilence(int numSamples)
{
    const GenericScopedLock<CriticalSection> lock(addingAudioSection);

    // Resize audio container
	audio->setSize(channels, numSamples, false, true, false);
	audio->clear();
	has_audio_data = true;

	// Calculate max audio sample added
	if (numSamples > max_audio_sample)
		max_audio_sample = numSamples;
}
