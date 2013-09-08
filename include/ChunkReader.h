#ifndef OPENSHOT_CHUNK_READER_H
#define OPENSHOT_CHUNK_READER_H

/**
 * \file
 * \brief Header file for ChunkReader class
 * \author Copyright (c) 2013 Jonathan Thomas
 */

#include "FileReaderBase.h"
#include "FFmpegReader.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <fstream>
#include <omp.h>
#include <qdir.h>
#include <stdio.h>
#include <stdlib.h>
#include <tr1/memory>
#include "Json.h"
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"

using namespace std;

namespace openshot
{

	/// This struct holds the location of a frame within a chunk. Chunks are small video files, which
	/// typically contain a few seconds of video each. Locating a frame among these small video files is
	/// accomplished by using this struct.
	struct chunk_location
	{
		int number;
		int frame;
	};

	enum ChunkVersion
	{
		THUMBNAIL,
		PREVIEW,
		FINAL
	};

	/**
	 * \brief This class reads a special chunk-formatted file, which can be easily
	 * shared in a distributed environment, and can return openshot::Frame objects.
	 */
	class ChunkReader : public FileReaderBase
	{
	private:
		string path;
		bool is_open;
		int chunk_size;
		FFmpegReader *local_reader;
		chunk_location previous_location;
		ChunkVersion version;
		tr1::shared_ptr<Frame> last_frame;

		/// Check if folder path existing
		bool does_folder_exist(string path);

		/// Find the location of a frame in a chunk
		chunk_location find_chunk_frame(int requested_frame);

		/// get a formatted path of a specific chunk
		string get_chunk_path(int chunk_number, string folder, string extension);

		/// Load JSON meta data about this chunk folder
		void load_json();

	public:

		/// Constructor for ChunkReader.  This automatically opens the chunk file or folder and loads
		/// frame 1, or it throws one of the following exceptions.
		ChunkReader(string path, ChunkVersion chunk_version) throw(InvalidFile, InvalidJSON);

		/// Close Reader
		void Close();

		/// Get the chunk size (number of frames to write in each chunk)
		int GetChunkSize() { return chunk_size; };

		/// Set the chunk size (number of frames to write in each chunk)
		int SetChunkSize(int new_size) { chunk_size = new_size; };

		/// Get an openshot::Frame object for a specific frame number of this reader.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed, ChunkNotFound);

		/// Open File - which is called by the constructor automatically
		void Open() throw(InvalidFile);
	};

}

#endif
