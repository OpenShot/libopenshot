
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <tr1/memory>
#include "../include/OpenShot.h"
#include <omp.h>

using namespace openshot;
using namespace tr1;

void FrameReady(int number)
{
	cout << "Frame #: " << number << " is ready!" << endl;
}

int main()
{

	/* TIMELINE ---------------- */
	Timeline t(720, 420, Framerate(24,1), 48000, 2);
	t.color.blue.AddPoint(1, 0);
	t.color.blue.AddPoint(300, 65000);

	// Add some clips
	Clip c1(new FFmpegReader("/home/jonathan/Videos/sintel_trailer-480p.mp4"));
	Clip c2(new FFmpegReader("/home/jonathan/Videos/sintel_trailer-480p.mp4"));
	Clip c3(new ImageReader("/home/jonathan/Desktop/icon.png"));

	// CLIP 1 (background movie)
	c1.Position(0.0);
	c1.gravity = GRAVITY_LEFT;
	c1.scale = SCALE_CROP;
	c1.Layer(0);

	// CLIP 2 (wave form)
	c2.Position(0.0);
	c2.Layer(1);
	c2.Waveform(true);
	c2.alpha.AddPoint(1, 1.0);
	c2.alpha.AddPoint(100, 0.0);
	c2.volume.AddPoint(1, 0.0);
	c2.volume.AddPoint(300, 0.0, LINEAR);

	// CLIP 3 (watermark)
	c3.Layer(2);
	c3.End(50);
	c3.gravity = GRAVITY_BOTTOM_RIGHT;
	c3.scale = SCALE_NONE;
	c3.alpha.AddPoint(1, 1.0);
	c3.alpha.AddPoint(75, 0.0);
	//c3.location_x = Keyframe(-5);
	//c3.location_y = Keyframe(-5);

	// Add clips
	t.AddClip(&c1);
	t.AddClip(&c2);
	t.AddClip(&c3);
	/* ---------------- */



	/* WRITER ---------------- */
	FFmpegWriter w("/home/jonathan/output.webm");

	// Set options
	w.SetAudioOptions(true, "libvorbis", 48000, 2, 128000);
	w.SetVideoOptions(true, "libvpx", Fraction(24,1), 720, 420, Fraction(1,1), false, false, 3000000);

	// Prepare Streams
	w.PrepareStreams();

	// Write header
	w.WriteHeader();

	// Output stream info
	w.OutputStreamInfo();

	for (int frame = 1; frame <= 300; frame++)
	{
		tr1::shared_ptr<Frame> f = t.GetFrame(frame);
		if (f)
		{
			//if (frame >= 13)
			//f->Display();

			// Write frame
			cout << "queue frame " << frame << " (" << f->number << ", " << f << ")" << endl;
			w.WriteFrame(f);
		}
	}

	// Write Footer
	w.WriteTrailer();

	// Close writer & reader
	w.Close();

	// Close timeline
	t.Close();
	/* ---------------- */


	cout << "Successfully Finished Timeline DEMO" << endl;
	return 0;

}

