#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FFmpegReader_Invalid_Path)
{
	// Check invalid path
	CHECK_THROW(FFmpegReader(""), InvalidFile);
}

TEST(FFmpegReader_GetFrame_Before_Opening)
{
	// Create a reader
	FFmpegReader r("../../src/examples/piano.wav");

	// Check invalid path
	CHECK_THROW(r.GetFrame(1), ReaderClosed);
}

TEST(FFmpegReader_Check_Audio_File)
{
	// Create a reader
	FFmpegReader r("../../src/examples/piano.wav");
	r.Open();

	// Get frame 1
	Frame *f = r.GetFrame(1);

	// Get the number of channels and samples
	float *samples = f->GetAudioSamples(0);

	// Check audio properties
	CHECK_EQUAL(2, f->GetAudioChannelsCount());
	CHECK_EQUAL(267, f->GetAudioSamplesCount());

	// Check actual sample values (to be sure the waveform is correct)
	CHECK_CLOSE(0.0f, samples[0], 0.00001);
	CHECK_CLOSE(0.0f, samples[50], 0.00001);
	CHECK_CLOSE(0.0f, samples[100], 0.00001);
	CHECK_CLOSE(0.0f, samples[200], 0.00001);
	CHECK_CLOSE(0.164062f, samples[230], 0.00001);
	CHECK_CLOSE(-0.164062f, samples[266], 0.00001);

	// Close reader
	r.Close();
}

TEST(FFmpegReader_Check_Video_File)
{
	// Create a reader
	FFmpegReader r("../../src/examples/test.mp4");
	r.Open();

	// Get frame 1
	Frame *f = r.GetFrame(1);

	// Get the image data
	const Magick::PixelPacket* pixels = f->GetPixels(10);

	// Check image properties on scanline 10, pixel 112
	CHECK_EQUAL(5397, pixels[112].red);
	CHECK_EQUAL(0, pixels[112].blue);
	CHECK_EQUAL(49087, pixels[112].green);
	CHECK_EQUAL(0, pixels[112].opacity);

	// Get frame 1
	f = r.GetFrame(2);

	// Get the next frame
	pixels = f->GetPixels(10);

	// Check image properties on scanline 10, pixel 112
	CHECK_EQUAL(0, pixels[112].red);
	CHECK_EQUAL(48316, pixels[112].blue);
	CHECK_EQUAL(24672, pixels[112].green);
	CHECK_EQUAL(0, pixels[112].opacity);

	// Close reader
	r.Close();

}
