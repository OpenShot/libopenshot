/**
 * \file
 * \brief Source code for the Frame class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "../include/Frame.h"

using namespace std;
using namespace openshot;

// Constructor - blank frame (300x200 blank image, 48kHz audio silence)
Frame::Frame() : number(1), image(0), audio(0), pixel_ratio(1,1), sample_rate(48000), channels(2)
{
	// Init the image magic and audio buffer
	image = new Magick::Image(Magick::Geometry(300,200), Magick::Color("red"));
	audio = new juce::AudioSampleBuffer(channels, 1600);

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Constructor - image only (48kHz audio silence)
Frame::Frame(int number, int width, int height, string color)
	: number(number), image(0), audio(0), pixel_ratio(1,1), sample_rate(48000), channels(2)
{
	// Init the image magic and audio buffer
	image = new Magick::Image(Magick::Geometry(width, height), Magick::Color(color));
	audio = new juce::AudioSampleBuffer(channels, 1600);

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Constructor - image only from pixel array (48kHz audio silence)
Frame::Frame(int number, int width, int height, const string map, const Magick::StorageType type, const void *pixels)
	: number(number), image(0), audio(0), pixel_ratio(1,1), sample_rate(48000), channels(2)
{
	// Init the image magic and audio buffer
	image = new Magick::Image(width, height, map, type, pixels);
	audio = new juce::AudioSampleBuffer(channels, 1600);

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Constructor - audio only (300x200 blank image)
Frame::Frame(int number, int samples, int channels) :
		number(number), image(0), audio(0), pixel_ratio(1,1), sample_rate(48000), channels(channels)
{
	// Init the image magic and audio buffer
	image = new Magick::Image(Magick::Geometry(300, 200), Magick::Color("white"));
	audio = new juce::AudioSampleBuffer(channels, samples);

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Constructor - image & audio
Frame::Frame(int number, int width, int height, string color, int samples, int channels)
	: number(number), image(0), audio(0), pixel_ratio(1,1), sample_rate(48000), channels(channels)
{
	// Init the image magic and audio buffer
	image = new Magick::Image(Magick::Geometry(width, height), Magick::Color(color));
	audio = new juce::AudioSampleBuffer(channels, samples);

	// initialize the audio samples to zero (silence)
	audio->clear();
};

// Destructor
Frame::~Frame()
{
	// deallocate image and audio memory
	DeletePointers();
}

// Copy constructor
Frame::Frame ( const Frame &other )
{
	// copy pointers and data
	DeepCopy(other);
}

// Assignment operator
Frame& Frame::operator= (const Frame& other)
{
	if (this != &other) {
		// deallocate image and audio memory
		DeletePointers();

		// copy pointers and data
		DeepCopy(other);
	}

	// return this instance
	return *this;
  }

// Copy data and pointers from another Frame instance
void Frame::DeepCopy(const Frame& other)
{
	// ignore copy if objects are the same
	number = other.number;
	image = new Magick::Image(*(other.image));
	audio = new juce::AudioSampleBuffer(*(other.audio));
	pixel_ratio = Fraction(other.pixel_ratio.num, other.pixel_ratio.den);
	sample_rate = other.sample_rate;
	channels = other.channels;
}

// Deallocate image and audio memory
void Frame::DeletePointers()
{
	// deallocate image memory
	delete image;
	image = NULL;
	// deallocate audio memory
	delete audio;
	audio = NULL;
}

// Display the frame image to the screen (primarily used for debugging reasons)
void Frame::Display()
{
	// Make a copy of the image (since we might resize it)
	Magick::Image copy;
	copy = *image;

	// display the image (if any)
	if (copy.size().width() > 1 && copy.size().height() > 1)
	{
		// Resize image (if needed)
		if (pixel_ratio.num != 1 || pixel_ratio.den != 1)
		{
			// Calculate correct DAR (display aspect ratio)
			int new_width = copy.size().width();
			int new_height = copy.size().height() * pixel_ratio.Reciprocal().ToDouble();

			// Resize image
			Magick::Geometry new_size(new_width, new_height);
			new_size.aspect(true);
			copy.resize(new_size);
		}

		// Disply image
		copy.display();
	}
}

// Display the wave form
void Frame::DisplayWaveform(bool resize)
{
	// Create blank image
	Magick::Image wave_image;

	// Init a list of lines
	list<Magick::Drawable> lines;
	lines.push_back(Magick::DrawableFillColor("#0070ff"));
	lines.push_back(Magick::DrawablePointSize(16));

	// Calculate the width of an image based on the # of samples
	int width = audio->getNumSamples();

	if (width > 0)
	{
		// If samples are present...
		int height = 200 * audio->getNumChannels();
		int height_padding = 20 * (audio->getNumChannels() - 1);
		int total_height = height + height_padding;
		wave_image = Magick::Image(Magick::Geometry(width, total_height), Magick::Color("#000000"));

		// Loop through each audio channel
		int Y = 100;
		for (int channel = 0; channel < audio->getNumChannels(); channel++)
		{
			// Get audio for this channel
			float *samples = audio->getSampleData(channel);

			for (int sample = 0; sample < audio->getNumSamples(); sample++)
			{
				// Sample value (scaled to -100 to 100)
				float value = samples[sample] * 100;

				if (value > 100 || value < -100)
				{
					cout << "TOO BIG: Sample # " << sample << " on frame " << number << " is TOO BIG: " << samples[sample] << endl;
				}

				// Append a line segment for each sample
				if (value != 0.0)
				{
					// LINE
					lines.push_back(Magick::DrawableStrokeColor("#0070ff"));
					lines.push_back(Magick::DrawableStrokeWidth(1));
					lines.push_back(Magick::DrawableLine(sample,Y, sample,Y-value)); // sample=X coordinate, Y=100 is the middle
				}
				else
				{
					// DOT
					lines.push_back(Magick::DrawableFillColor("#0070ff"));
					lines.push_back(Magick::DrawableStrokeWidth(1));
					lines.push_back(Magick::DrawablePoint(sample,Y));
				}
			}

			// Add Channel Label
			stringstream label;
			label << "Channel " << channel;
			lines.push_back(Magick::DrawableStrokeColor("#ffffff"));
			lines.push_back(Magick::DrawableFillColor("#ffffff"));
			lines.push_back(Magick::DrawableStrokeWidth(0.1));
			lines.push_back(Magick::DrawableText(5, Y - 5, label.str()));

			// Increment Y
			Y += (200 + height_padding);
		}

		// Draw the waveform
		wave_image.draw(lines);

		// Resize Image (if requested)
		if (resize)
			// Resize to 60%
			wave_image.resize(Magick::Geometry(width * 0.6, total_height * 0.6));
	}
	else
	{
		// No audio samples present
		wave_image = Magick::Image(Magick::Geometry(720, 480), Magick::Color("#000000"));

		// Add Channel Label
		lines.push_back(Magick::DrawableStrokeColor("#ffffff"));
		lines.push_back(Magick::DrawableFillColor("#ffffff"));
		lines.push_back(Magick::DrawableStrokeWidth(0.1));
		lines.push_back(Magick::DrawableText(265, 240, "No Audio Samples Found"));

		// Draw the waveform
		wave_image.draw(lines);
	}

	// Display Image
	wave_image.display();
}

// Get an array of sample data
float* Frame::GetAudioSamples(int channel)
{
	// return JUCE audio data for this channel
	return audio->getSampleData(channel);
}

// Get an array of sample data (all channels interleaved together), using any sample rate
float* Frame::GetInterleavedAudioSamples(int new_sample_rate, int* sample_count)
{
	float *output = NULL;
	AudioSampleBuffer *buffer = audio;
	AudioSampleBuffer *resampled_buffer = NULL;
	int num_of_channels = audio->getNumChannels();
	int num_of_samples = audio->getNumSamples();

//	DEBUG CODE
//	if (number == 1)
//		for (int s = 0; s < num_of_samples; s++)
//			for (int c = 0; c < num_of_channels; c++)
//				cout << buffer->getSampleData(c)[s] << endl;

	// Resample to new sample rate (if needed)
	if (new_sample_rate != sample_rate)
	{
		// YES, RESAMPLE AUDIO

		// Get the frame's audio source
		AudioBufferSource my_source(audio);

		// Create resample source
		ResamplingAudioSource resample_source(&my_source, false, num_of_channels);

		// Set the sample ratio (original to new sample rate)
		double source_ratio = double(sample_rate) / double(new_sample_rate);
		double dest_ratio = double(new_sample_rate) / double(sample_rate);
		int new_num_of_samples = num_of_samples * dest_ratio;
		resample_source.setResamplingRatio(source_ratio);

		// Prepare to resample
		resample_source.prepareToPlay(num_of_samples, new_sample_rate);

		// Create a buffer for the newly resampled data
		resampled_buffer = new AudioSampleBuffer(num_of_channels, new_num_of_samples);
		const AudioSourceChannelInfo resample_callback_buffer = {resampled_buffer, 0, new_num_of_samples};
		resample_callback_buffer.clearActiveBufferRegion();

		// Resample the current frame's audio buffer (info the temp callback buffer)
		resample_source.getNextAudioBlock(resample_callback_buffer);

		// Update buffer pointer to this newly resampled buffer
		buffer = resampled_buffer;

		// Update num_of_samples
		num_of_samples = new_num_of_samples;
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
			//cout << position << ", " << channel << ", " << sample << endl;
			output[position] = buffer->getSampleData(channel)[sample];

			// increment position
			position++;
		}
	}

	// Clean up
	delete resampled_buffer;

	// Update sample count (since it might have changed due to resampling)
	*sample_count = num_of_samples;

	// return combined array
	return output;
}

// Get number of audio channels
int Frame::GetAudioChannelsCount()
{
	return audio->getNumChannels();
}

// Get number of audio samples
int Frame::GetAudioSamplesCount()
{
	return audio->getNumSamples();
}

// Get the audio sample rate
int Frame::GetAudioSamplesRate()
{
	return sample_rate;
}

// Get pixel data (as packets)
const Magick::PixelPacket* Frame::GetPixels()
{
	// Return arry of pixel packets
	return image->getConstPixels(0,0, image->columns(), image->rows());
}

// Get pixel data (for only a single scan-line)
const Magick::PixelPacket* Frame::GetPixels(int row)
{
	// Return arry of pixel packets
	return image->getConstPixels(0,row, image->columns(), 1);
}

// Get pixel data (for a resized image)
const Magick::PixelPacket* Frame::GetPixels(unsigned int width, unsigned int height, int frame)
{
	// Create a new resized image
	//Magick::Image newImage = *image;
	small_image = new Magick::Image(*(image));
	small_image->resize(Magick::Geometry(width, height));
	small_image->colorize(255, 0, 0, Magick::Color(0,0,255));
	small_image->blur(5.0, 5.0);

	stringstream file;
	file << "frame" << frame << ".png";
	small_image->write(file.str());

	// Return arry of pixel packets
	return small_image->getConstPixels(0,0, small_image->columns(), small_image->rows());
}

// Set Pixel Aspect Ratio
void Frame::SetPixelRatio(int num, int den)
{
	pixel_ratio.num = num;
	pixel_ratio.den = den;
}

// Set Sample Rate
void Frame::SetSampleRate(int rate)
{
	sample_rate = rate;
}

// Get height of image
int Frame::GetHeight()
{
	// return height
	return image->rows();
}

// Get height of image
int Frame::GetWidth()
{
	// return width
	return image->columns();
}

// Save the frame image
void Frame::Save()
{
	// save the image
	stringstream file;
	file << "frame" << number << ".png";
	image->write(file.str());
}

// Add (or replace) pixel data to the frame
void Frame::AddImage(int width, int height, const string map, const Magick::StorageType type, const void *pixels)
{
	// Deallocate image memory
	delete image;
	image = NULL;

	// Create new image object, and fill with pixel data
	image = new Magick::Image(width, height, map, type, pixels);
}

// Add audio samples to a specific channel
void Frame::AddAudio(int destChannel, int destStartSample, const float* source, int numSamples, float gainToApplyToSource = 1.0f)
{
	// Add samples to frame's audio buffer
	audio->addFrom(destChannel, destStartSample, source, numSamples, gainToApplyToSource);
}

// Play audio samples for this frame
void Frame::Play()
{
	// Check if samples are present
	if (!audio->getNumSamples())
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
	my_source = new AudioBufferSource(audio);

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
		sleep(1);
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





