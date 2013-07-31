#ifndef OPENSHOT_CHUNK_READER_H
#define OPENSHOT_CHUNK_READER_H

/**
 * \file
 * \brief Header file for ChunkReader class
 * \author Copyright (c) 2013 Jonathan Thomas
 */

#include "FileReaderBase.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <tr1/memory>
#include "Magick++.h"
#include "Cache.h"
#include "Exceptions.h"

using namespace std;

namespace openshot
{

	/**
	 * \brief This class reads a special chunk-formatted file, which can be easily
	 * shared in a distributed environment, and can return openshot::Frame objects.
	 */
	class ChunkReader : public FileReaderBase
	{
	private:
		string path;
		bool is_open;

	public:

		/// Constructor for ChunkReader.  This automatically opens the chunk file or folder and loads
		/// frame 1, or it throws one of the following exceptions.
		ChunkReader(string path) throw(InvalidFile);

		/// Close Reader
		void Close();

		/// Get an openshot::Frame object for a specific frame number of this reader.
		///
		/// @returns The requested frame (containing the image)
		/// @param[requested_frame] number The frame number that is requested.
		tr1::shared_ptr<Frame> GetFrame(int requested_frame) throw(ReaderClosed);

		/// Open File - which is called by the constructor automatically
		void Open() throw(InvalidFile);
	};

}

#endif
