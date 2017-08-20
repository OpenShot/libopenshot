/**
 * @file
 * @brief Header file for ChunkWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_CHUNK_WRITER_H
#define OPENSHOT_CHUNK_WRITER_H

#include "ReaderBase.h"
#include "WriterBase.h"
#include "FFmpegWriter.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <fstream>
#include <omp.h>
#include <QtCore/qdir.h>
#include <stdio.h>
#include <sstream>
#include <unistd.h>
#include "CacheMemory.h"
#include "Exceptions.h"
#include "Json.h"


using namespace std;

namespace openshot
{
	/**
	 * @brief This class takes any reader and generates a special type of video file, built with
	 * chunks of small video and audio data.
	 *
	 * These chunks can easily be passed around in a distributed
	 * computing environment, without needing to share the entire video file. They also allow a
	 * chunk to be frame accurate, since seeking inaccuracies are removed.
	 *
	 * @code
	 * // This example demonstrates how to feed a reader into a ChunkWriter
	 * FFmpegReader *r = new FFmpegReader("MyAwesomeVideo.mp4"); // Get a reader
	 * r.Open(); // Open the reader
	 *
	 * // Create a ChunkWriter (and a folder location on your computer)
	 * ChunkWriter w("/folder_path_to_hold_chunks/", r);
	 *
	 * // Open the writer
	 * w.Open();
	 *
	 * // Write a block of frames to the ChunkWriter (from frame 1 to the end)
	 * w.WriteFrame(r, 1, r->info.video_length);
	 *
	 * // Close the reader & writer
	 * w.Close();
	 * r.Close();
	 * @endcode
	 */
	class ChunkWriter : public WriterBase
	{
	private:
		string path;
		int chunk_count;
		int chunk_size;
		int frame_count;
		bool is_open;
		bool is_writing;
		ReaderBase *local_reader;
		FFmpegWriter *writer_thumb;
		FFmpegWriter *writer_preview;
		FFmpegWriter *writer_final;
	    std::shared_ptr<Frame> last_frame;
	    bool last_frame_needed;
	    string default_extension;
	    string default_vcodec;
	    string default_acodec;

		/// check for chunk folder
		void create_folder(string path);

		/// get a formatted path of a specific chunk
		string get_chunk_path(int chunk_number, string folder, string extension);

		/// check for valid chunk json
		bool is_chunk_valid();

		/// write json meta data
		void write_json_meta_data();

	public:

		/// @brief Constructor for ChunkWriter. Throws one of the following exceptions.
		/// @param path The folder path of the chunk file to be created
		/// @param reader The initial reader to base this chunk file's meta data on (such as fps, height, width, etc...)
		ChunkWriter(string path, ReaderBase *reader) throw(InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory);

		/// Close the writer
		void Close();

		/// Get the chunk size (number of frames to write in each chunk)
		int GetChunkSize() { return chunk_size; };

		/// Determine if writer is open or closed
		bool IsOpen() { return is_open; };

		/// Open writer
		void Open() throw(InvalidFile, InvalidCodec);

		/// @brief Set the chunk size (number of frames to write in each chunk)
		/// @param new_size The number of frames to write in this chunk file
		void SetChunkSize(int new_size) { chunk_size = new_size; };

		/// @brief Add a frame to the stack waiting to be encoded.
		/// @param frame The openshot::Frame object that needs to be written to this chunk file.
		void WriteFrame(std::shared_ptr<Frame> frame) throw(WriterClosed);

		/// @brief Write a block of frames from a reader
		/// @param start The starting frame number to write (of the reader passed into the constructor)
		/// @param length The number of frames to write (of the reader passed into the constructor)
		void WriteFrame(int start, int length) throw(WriterClosed);

		/// @brief Write a block of frames from a reader
		/// @param reader The reader containing the frames you need
		/// @param start The starting frame number to write
		/// @param length The number of frames to write
		void WriteFrame(ReaderBase* reader, int start, int length) throw(WriterClosed);

	};

}

#endif
