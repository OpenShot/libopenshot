
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

//	FFmpegReader r("/home/jonathan/Videos/sintel_trailer-1080p.mp4");
//	r.Open();
//	tr1::shared_ptr<Frame> f = r.GetFrame(300);
//	f->Display();
//	r.Close();
//
//	return 0;



	/* TIMELINE ---------------- */
	Timeline t(1280, 720, Framerate(25,1), 44100, 2);


	// Add some clips
	Clip c1(new ImageReader("/home/jonathan/Desktop/Template2/video_background.jpg"));
	Clip c2(new FFmpegReader("/home/jonathan/Desktop/Template2/OpenShot Version 1-4-3.mp4"));
//	Clip c2(new FFmpegReader("/home/jonathan/Videos/sintel_trailer-480p.mp4"));
	Clip c3(new ImageReader("/home/jonathan/Desktop/Template2/logo.png"));
//	Clip c4(new ImageReader("/home/jonathan/Desktop/text3.png")); // audio
//	Clip c5(new ImageReader("/home/jonathan/Desktop/text1.png")); // color
//	Clip c6(new ImageReader("/home/jonathan/Desktop/text4.png")); // sub-pixel
//	Clip c7(new ImageReader("/home/jonathan/Desktop/text8.png")); // coming soon
//	Clip c10(new ImageReader("/home/jonathan/Desktop/text10.png")); // time mapping

	// CLIP 1 (background movie)
	c1.Position(0.0);
	c1.gravity = GRAVITY_LEFT;
	c1.scale = SCALE_CROP;
	c1.Layer(0);
	//c1.End(16.0);
	//c1.alpha.AddPoint(1, 0.0);
	//c1.alpha.AddPoint(500, 0.0);
	//c1.alpha.AddPoint(565, 1.0);
//	c1.time.AddPoint(1, 3096);
//	c1.time.AddPoint(180, 3276, LINEAR);
//	c1.time.AddPoint(181, 3276);
//	c1.time.AddPoint(361, 3096, LINEAR);
//	c1.time.AddPoint(362, 3096);
//	c1.time.AddPoint(722, 3276, LINEAR);
//	c1.time.AddPoint(723, 3276);
//	c1.time.AddPoint(903, 2916, LINEAR);
//	c1.time.AddPoint(1083, 3276, LINEAR);
	c1.time.PrintValues();
	//return 1;


	// CLIP 2 (wave form)
	c2.Position(0.0);
	c2.Layer(1);
	c2.Waveform(true);
//	c2.alpha.AddPoint(1, 1.0);
//	c2.alpha.AddPoint(150, 0.0);
//	c2.alpha.AddPoint(360, 0.0, LINEAR);
//	c2.alpha.AddPoint(384, 1.0);
	c2.volume.AddPoint(1, 1.0);
	c2.volume.AddPoint(900, 1.0, LINEAR);
	c2.volume.AddPoint(1000, 0.0, LINEAR);
//	c2.End(15.0);
	c2.wave_color.blue.AddPoint(1, 65000);
	c2.wave_color.blue.AddPoint(300, 0);
	c2.wave_color.red.AddPoint(1, 0);
	c2.wave_color.red.AddPoint(300, 65000);

	// CLIP 3 (watermark)
	c3.Layer(2);
//	c3.End(50);
	c3.gravity = GRAVITY_CENTER;
	c3.scale = SCALE_NONE;
	c3.alpha.AddPoint(1, 1.0);
	c3.alpha.AddPoint(150, 0.0);
//	c3.location_x.AddPoint(1, -5);
//	c3.location_y.AddPoint(1, -5);
//
//	// CLIP 4 (text about waveform)
//	c4.Layer(3);
//	c4.scale = SCALE_NONE;
//	c4.gravity = GRAVITY_CENTER;
//	c4.Position(1);
//	c4.alpha.AddPoint(1, 1.0);
//	c4.alpha.AddPoint(30, 0.0);
//	c4.alpha.AddPoint(100, 0.0);
//	c4.alpha.AddPoint(130, 1.0);
//	c4.End(5.5);
//
//	// CLIP 5 (text about colors)
//	c5.Layer(3);
//	c5.scale = SCALE_NONE;
//	c5.gravity = GRAVITY_CENTER;
//	c5.Position(16);
//	c5.alpha.AddPoint(1, 1.0);
//	c5.alpha.AddPoint(30, 0.0);
//	c5.alpha.AddPoint(100, 0.0);
//	c5.alpha.AddPoint(130, 1.0);
//	c5.End(5.5);
//
//	// CLIP 6 (text about sub-pixel)
//	c6.Layer(3);
//	c6.scale = SCALE_NONE;
//	c6.gravity = GRAVITY_LEFT;
//	c6.Position(24);
//	c6.alpha.AddPoint(1, 1.0);
//	c6.alpha.AddPoint(30, 0.0);
//	c6.alpha.AddPoint(100, 0.0);
//	c6.alpha.AddPoint(130, 1.0);
//	c6.location_x.AddPoint(1,0.05);
//	c6.location_x.AddPoint(130, 0.3);
//	c6.End(5.5);
//
//	// CLIP 7 (text about coming soon)
//	c7.Layer(3);
//	c7.scale = SCALE_NONE;
//	c7.gravity = GRAVITY_CENTER;
//	c7.Position(18.0);
//	c7.alpha.AddPoint(1, 1.0);
//	c7.alpha.AddPoint(30, 0.0);
//	c7.alpha.AddPoint(100, 0.0);
//	c7.alpha.AddPoint(130, 1.0);
//	c7.End(5.5);
//
//
//	// CLIP 10 (text about waveform)
//	c10.Layer(3);
//	c10.scale = SCALE_NONE;
//	c10.gravity = GRAVITY_CENTER;
//	c10.Position(1);
//	c10.alpha.AddPoint(1, 1.0);
//	c10.alpha.AddPoint(30, 0.0);
//	c10.alpha.AddPoint(100, 0.0);
//	c10.alpha.AddPoint(130, 1.0);
//	c10.End(5.5);

	// Add clips
	t.AddClip(&c1);
	t.AddClip(&c2);
	t.AddClip(&c3);
	//t.AddClip(&c4);
	//t.AddClip(&c5);
	//t.AddClip(&c6);
	//t.AddClip(&c7);
	//t.AddClip(&c10);
	/* ---------------- */



	/* WRITER ---------------- */
	FFmpegWriter w("/home/jonathan/output.mp4");

	// Set options
	//w.SetAudioOptions(true, "libvorbis", 48000, 2, 188000);
	w.SetAudioOptions(true, "libmp3lame", 44100, 2, 128000);
	//w.SetVideoOptions(true, "libvpx", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 30000000);
	w.SetVideoOptions(true, "mpeg4", Fraction(25,1), 1280, 720, Fraction(1,1), false, false, 30000000);

	// Prepare Streams
	w.PrepareStreams();

	// Write header
	w.WriteHeader();

	// Output stream info
	w.OutputStreamInfo();

	//for (int frame = 3096; frame <= 3276; frame++)
	for (int frame = 1; frame <= 1000; frame++)
	{
		tr1::shared_ptr<Frame> f = t.GetFrame(frame);
		if (f)
		{
			//if (frame >= 62)
			//f->DisplayWaveform();
			//f->AddOverlayNumber(frame);
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

