#ifndef OPENSHOT_CHUNK_WRITER_H
#define OPENSHOT_CHUNK_WRITER_H

/**
 * \file
 * \brief Header file for ChunkWriter class
 * \author Copyright (c) 2013 Jonathan Thomas
 */

#include "FileReaderBase.h"
#include "FileWriterBase.h"
#include "FFmpegWriter.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <fstream>
#include <omp.h>
#include <qdir.h>
#include <stdio.h>
#include <sstream>
#include <unistd.h>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"
#include "Json.h"
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
		int chunk_count;
		int chunk_size;
		int frame_count;
		bool is_writing;
		FileReaderBase *local_reader;
		FFmpegWriter *writer_thumb;
		FFmpegWriter *writer_preview;
		FFmpegWriter *writer_final;
	    tr1::shared_ptr<Frame> last_frame;
	    string default_extension;
	    string default_vcodec;
	    string default_acodec;

		/// check for chunk folder
		bool create_folder(string path);

		/// get a formatted path of a specific chunk
		string get_chunk_path(int chunk_number, string folder, string extension);

		/// check for valid chunk json
		bool is_chunk_valid();

		/// write json meta data
		void write_json_meta_data();

	public:

		/// Constructor for ChunkWriter. Throws one of the following exceptions.
		ChunkWriter(string path, FileReaderBase *reader) throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory);

		/// Close the writer
		void Close();

		/// Get the chunk size (number of frames to write in each chunk)
		int GetChunkSize() { return chunk_size; };

		/// Set the chunk size (number of frames to write in each chunk)
		int SetChunkSize(int new_size) { chunk_size = new_size; };

		/// Add a frame to the stack waiting to be encoded.
		void WriteFrame(tr1::shared_ptr<Frame> frame);

		/// Write a block of frames from a reader
		void WriteFrame(int start, int length);

		/// Write a block of frames from a reader
		void WriteFrame(FileReaderBase* reader, int start, int length);

	};

}

#endif
