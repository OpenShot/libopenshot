#ifndef OPENSHOT_CHUNK_WRITER_H
#define OPENSHOT_CHUNK_WRITER_H

/**
 * \file
 * \brief Header file for ChunkWriter class
 * \author Copyright (c) 2013 Jonathan Thomas
 */

#include "FileReaderBase.h"
#include "FileWriterBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <sstream>
#include <unistd.h>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"
#include "Sleep.h"


using namespace std;

namespace openshot
{
	/**
	 * \brief This class takes any reader and generates a special type of video file, built with
	 * chunks of small video and audio data. These chunks can easily be passed around in a distributed
	 * computing environment, without needing to share the entire video file.
	 */
	class ChunkWriter : public FileWriterBase
	{
	private:
		string path;
		int cache_size;
		bool is_writing;
		int64 write_video_count;
		int64 write_audio_count;

	    tr1::shared_ptr<Frame> last_frame;
	    deque<tr1::shared_ptr<Frame> > spooled_frames;
	    deque<tr1::shared_ptr<Frame> > queued_frames;
	    deque<tr1::shared_ptr<Frame> > processed_frames;

		/// process video frame
		void process_frame(tr1::shared_ptr<Frame> frame);

		/// write all queued frames
		void write_queued_frames();

	public:

		/// Constructor for ChunkWriter. Throws one of the following exceptions.
		ChunkWriter(FileReaderBase *reader, string path) throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory);

		/// Close the writer
		void Close();

		/// Get the cache size (number of frames to queue before writing)
		int GetCacheSize() { return cache_size; };

		/// Set the cache size (number of frames to queue before writing)
		int SetCacheSize(int new_size) { cache_size = new_size; };

		/// Add a frame to the stack waiting to be encoded.
		void WriteFrame(tr1::shared_ptr<Frame> frame);

		/// Write a block of frames from a reader
		void WriteFrame(FileReaderBase* reader, int start, int length);

	};

}

#endif
