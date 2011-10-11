// tutorial01.c
// Code based on a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1

// A small sample program that shows how to use libavformat and libavcodec to
// read video from a file.
//
// Use
//
// gcc -o tutorial01 tutorial01.c -lavformat -lavcodec -lz
//
// to build (assuming libavformat and libavcodec are correctly installed
// your system).
//
// Run using
//
// tutorial01 myvideofile.mpg
//
// to write the first five frames from "myvideofile.mpg" to disk in PPM
// format.

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
}

#include <iostream>
#include <stdio.h>
#include <ctime>
#include <omp.h>

#include "Magick++.h"

using namespace std;
using namespace Magick;

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
	FILE *pFile;
	char szFilename[32];
	int y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;time_t seconds;

	  seconds = time (NULL);

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}

/// Process a single frame of video
int process_frame(AVPicture *copyFrame, int original_width, int original_height, PixelFormat pix_fmt, int i)
{
	//cout << i << endl;

	AVFrame *pFrameRGB = NULL;
	int numBytes;
	uint8_t *buffer = NULL;

	// Allocate an AVFrame structure
	pFrameRGB = avcodec_alloc_frame();
	if (pFrameRGB == NULL)
		return -1;

	// Determine required buffer size and allocate buffer
	const int new_width = 640;
	const int new_height = 350;
	numBytes = avpicture_get_size(PIX_FMT_RGB24, new_width,
			new_height);
	buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *) pFrameRGB, buffer, PIX_FMT_RGB24,
			new_width, new_height);

	struct SwsContext *img_convert_ctx = NULL;

	// Convert the image into YUV format that SDL uses
	if(img_convert_ctx == NULL) {
	     img_convert_ctx = sws_getContext(original_width, original_height,
	    		 pix_fmt, new_width, new_height,
	                      PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	     if(img_convert_ctx == NULL) {
	      fprintf(stderr, "Cannot initialize the conversion context!\n");
	      exit(1);
	     }

	}

	sws_scale(img_convert_ctx, copyFrame->data, copyFrame->linesize,
	 0, original_height, pFrameRGB->data, pFrameRGB->linesize);

	// Get ImageMagick Constructor
	Image img(new_width, new_height, "RGB", CharPixel, buffer);

	// Apply some ImageMagick filters
	img.negate();
	//img.sigmoidalContrast(15.0, 0.30 );
	//img.sparseColor(AllChannels, BarycentricColorInterpolate, 0, args);
	img.flip();

	// Get a list of pixels in our newly modified image.  Each pixel is represented by
	// a PixelPacket struct, which has 4 properties: .red, .blue, .green, .alpha
	PixelPacket *pixel_packets = img.getPixels(0,0,new_width, new_height);

	// loop through ImageMagic pixel structs, and put the colors in a regular array, and move the
	// colors around to match FFmpeg's order (RGB).  They appear to be in RBG by default.
	for (int packet = 0, row = 0; row < numBytes; packet++, row+=3)
	{
		// Update buffer (which is already linked to the AVFrame: pFrameRGB)
		buffer[row] = pixel_packets[packet].red;
		buffer[row+1] = pixel_packets[packet].green;
		buffer[row+2] = pixel_packets[packet].blue;
	}

	if (i % 5 == 0 and i != 0)
		cout << "5 Frames Processed!" << endl;
		//cout << i << endl;

	// write back pixels to AVFrame
	if (i > 1 && i < 10)
		SaveFrame(pFrameRGB, new_width, new_height, i);
		//img.write("/home/jonathan/Aptana Studio Workspace/LearnFFmpeg/frameXX.png");


	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);

	return 0;
}

int main(int argc, char *argv[]) {

	AVFormatContext *pFormatCtx;
	int i, videoStream;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;
	AVPacket packet;
	int frameFinished;

	if (argc < 2) {
		printf("Please provide a movie file\n");
		return -1;
	}
	// Register all formats and codecs
	av_register_all();

	// Open video file
	if (av_open_input_file(&pFormatCtx, argv[1], NULL, 0, NULL) != 0)
		return -1; // Couldn't open file

	// Retrieve stream information
	if (av_find_stream_info(pFormatCtx) < 0)
		return -1; // Couldn't find stream information

	// Dump information about file onto standard error
	dump_format(pFormatCtx, 0, argv[1], 0);

	// Find the first video stream
	videoStream = -1;
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if (avcodec_open(pCodecCtx, pCodec) < 0)
		return -1; // Could not open codec

	// Allocate video frame
	AVFrame *pFrame = avcodec_alloc_frame();

	#pragma omp parallel private(i)
	{
		#pragma omp master
		{

			// Read frames and save first five frames to disk
			i = 0;
			while (av_read_frame(pFormatCtx, &packet) >= 0)
			{
				// Is this a packet from the video stream?
				if (packet.stream_index == videoStream)
				{
					// Decode video frame
					avcodec_decode_video(pCodecCtx, pFrame, &frameFinished,
							packet.data, packet.size);

					// Did we get a video frame?
					if (frameFinished)
					{

						#pragma omp task
						{

							// Get the array buffer
							int w = pCodecCtx->width;
							int h = pCodecCtx->height;

							AVPicture copyFrame;
							avpicture_alloc(&copyFrame, pCodecCtx->pix_fmt, w, h);
							av_picture_copy(&copyFrame, (AVPicture *) pFrame, pCodecCtx->pix_fmt, w, h);

							// Process Frame
							process_frame(&copyFrame, w, h, pCodecCtx->pix_fmt, i);

							// Free AVPicture
							avpicture_free(&copyFrame);
						}


					}

					i++;
				}


			}

		}
	}

	// Free the packet that was allocated by av_read_frame
	av_free_packet(&packet);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	av_close_input_file(pFormatCtx);

	cout << "Done!" << endl;

	return 0;
}
