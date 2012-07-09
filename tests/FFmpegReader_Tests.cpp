#include "UnitTest++.h"
#include "../include/OpenShot.h"

using namespace std;
using namespace openshot;

TEST(FFmpegReader_Invalid_Path)
{
	// Check invalid path
	CHECK_THROW(FFmpegReader r(""), InvalidFile);
}

TEST(FFmpegReader_Check_Audio_File)
{
	// Create a reader
	FFmpegReader r("../../src/examples/piano.wav");

	// Get frame 1
	Frame f = r.GetFrame(1);

	// Get the number of channels and samples
	float *samples = f.GetAudioSamples(0);

	// Check audio properties
	CHECK_EQUAL(2, f.GetAudioChannelsCount());
	CHECK_EQUAL(267, f.GetAudioSamplesCount());

	// Check actual sample values (to be sure the waveform is correct)
	CHECK_CLOSE(0.0f, samples[0], 0.00001);
	CHECK_CLOSE(0.0f, samples[50], 0.00001);
	CHECK_CLOSE(0.0f, samples[100], 0.00001);
	CHECK_CLOSE(0.0f, samples[200], 0.00001);
	CHECK_CLOSE(0.164062f, samples[230], 0.00001);
	CHECK_CLOSE(-0.164062f, samples[266], 0.00001);
}

