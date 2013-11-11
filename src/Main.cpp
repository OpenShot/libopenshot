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
#include <qt4/QtCore/qdir.h>
#include <qt4/QtMultimedia/qvideoframe.h>
#include <qt4/QtMultimedia/qvideosurfaceformat.h>
#include <QtGui/QApplication>

using namespace openshot;
using namespace tr1;

int main(int argc, char* argv[])
{

	// Start Qt Application
	QApplication app(argc, argv);\

	// Init video player widget
	VideoPlayer player;
	player.showMaximized();

	// Prepare video surface
	VideoWidgetSurface * videoWidget = new VideoWidgetSurface(&player);
	QSize videoSize(1280, 720); // supplement with your video dimensions

	// look at VideoWidgetSurface::supportedPixelFormats for supported formats
	QVideoSurfaceFormat format( videoSize, QVideoFrame::Format_ARGB32);



	// Get test frame
	FFmpegReader r2("/home/jonathan/Videos/sintel_trailer-720p.mp4");
	r2.Open();
	tr1::shared_ptr<Frame> frame = r2.GetFrame(600);

	// Get image
	tr1::shared_ptr<Magick::Image> image = r2.GetFrame(300)->GetImage();

	// Create Qt Video Frame
	QVideoFrame aFrame(32 * image->size().width() * image->size().height(), QSize(image->size().width(), image->size().height()), 32 * image->size().width(), QVideoFrame::Format_ARGB32);

	// Get a reference to the internal videoframe buffer
	aFrame.map(QAbstractVideoBuffer::WriteOnly);
	uchar *pixels = aFrame.bits();

	// Copy pixel data from ImageMagick to Qt
	Magick::Blob my_blob_1;
	image->write(&my_blob_1); // or PNG img1.write(&my_blob_1); const QByteArray imageData1((char*)(my_blob_1.data()),my_blob_1.length());

	pixels = (uchar*)(my_blob_1.data()),my_blob_1.length();
	//memcpy(pixels, my_blob_1.data(), my_blob_1.length());

	// Get a list of pixels from source image
//	const Magick::PixelPacket *pixel_packets = frame->GetPixels();
//
//	// Fill the AVFrame with RGB image data
//	int source_total_pixels = image->size().width() * image->size().height();
//	for (int packet = 0, row = 0; packet < source_total_pixels; packet++, row+=4)
//	{
//		// Update buffer (which is already linked to the AVFrame: pFrameRGB)
//		// Each color needs to be 8 bit (so I'm bit shifting the 16 bit ints)
//		pixels[row] = 255;
//		pixels[row+1] = 255;
//		pixels[row+2] = pixel_packets[packet].green >> 8;
//		pixels[row+3] = pixel_packets[packet].blue >> 8;
//		//pixels[row] = qRgb(pixel_packets[packet].red, pixel_packets[packet].green, pixel_packets[packet].blue);
//	}
	aFrame.unmap();

	// Start video player (which sets format's size)
	videoWidget->start(format);

	// Display frame
	videoWidget->present(aFrame);



	// Start Qt App
    return app.exec();








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

