/**
 * @file
 * @brief Source file for Main class (example app for libopenshot)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2013 OpenShot Studios, LLC
 * (http://www.openshotstudios.com). This file is part of
 * OpenShot Library (http://www.openshot.org), an open-source project
 * dedicated to delivering high quality video editing and animation solutions
 * to the world.
 *
 * OpenShot Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenShot Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenShot Library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <tr1/memory>
#include "../include/OpenShot.h"
#include "../include/Json.h"
#include <omp.h>


using namespace openshot;
using namespace tr1;

int main(int argc, char* argv[])
{


//	FFmpegReader r2("/home/jonathan/Videos/sintel_trailer-720p.mp4");
//	r2.Open();
//	SDLPlayer p;
//	p.Reader(&r2);
//	p.Play();
//	return 0;



	// Image of interlaced frame
//	ImageReader ir("/home/jonathan/apps/libopenshot/src/examples/interlaced.png");
//	ir.Open();
//
//	// FrameMapper to de-interlace frame
//	//FrameMapper fm(&ir, Framerate(24,1), PULLDOWN_NONE);
//	//fm.DeInterlaceFrame(ir.GetFrame(1), true)->Display();
//	Deinterlace de(false);
//	de.GetFrame(ir.GetFrame(1), 1)->Display();
//
//
//	return 0;


	// Reader
	FFmpegReader r1("/home/jonathan/colors-24-converted-to-29-97-fps-pulldown-advanced.mp4");
	r1.Open();

	// FrameMapper
	FrameMapper r(&r1, Framerate(24,1), PULLDOWN_ADVANCED);
	r.PrintMapping();

	/* WRITER ---------------- */
	FFmpegWriter w("/home/jonathan/output.mp4");

	// Set options
	//w.SetAudioOptions(true, "libvorbis", 48000, 2, 188000);
	//w.SetAudioOptions(true, "libmp3lame", 44100, 2, 128000);
	//w.SetVideoOptions(true, "libvpx", Fraction(24,1), 1280, 720, Fraction(1,1), false, false, 30000000);
	w.SetVideoOptions(true, "mpeg4", r.info.fps, 1280, 720, Fraction(1,1), false, false, 3000000);

	// Prepare Streams
	w.PrepareStreams();

	// Write header
	w.WriteHeader();

	// Output stream info
	w.OutputStreamInfo();

	//for (int frame = 3096; frame <= 3276; frame++)
	for (int frame = 1; frame <= 20; frame++)
	{
//		tr1::shared_ptr<Frame> f(new Frame(frame, 1280, 720, "#000000", 44100, 2));
//		if (frame % 2 == 0)
//			f->AddColor(1280, 720, "Yellow");
//		else
//			f->AddColor(1280, 720, "Black");
//
//		f->AddOverlayNumber(f->number);
//		cout << f->number << endl;
//		w.WriteFrame(f);

		tr1::shared_ptr<Frame> f = r.GetFrame(frame);
		if (f)
		{
			//if (frame >= 250)
			//	f->DisplayWaveform();
			//f->AddOverlayNumber(frame);
			//f->Display();

			// Write frame
			f->Display();
			cout << "queue frame " << frame << " (" << f->number << ", " << f << ")" << endl;
			w.WriteFrame(f);
		}
	}

	// Write Footer
	w.WriteTrailer();

	// Close writer & reader
	w.Close();

	// Close timeline
	r1.Close();
	/* ---------------- */


	cout << "Successfully Finished Timeline DEMO" << endl;
	return 0;

}

