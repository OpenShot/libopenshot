/**
 * @file
 * @brief Header file for ChunkReader class
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

#ifndef OPENSHOT_CHUNK_READER_H
#define OPENSHOT_CHUNK_READER_H

#include "ReaderBase.h"
#include "FFmpegReader.h"

#include <cmath>
#include <ctime>
#include <iostream>
#include <fstream>
#include <omp.h>
#include <QtCore/qdir.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include "Json.h"
#include "CacheMemory.h"
#include "Exceptions.h"

using namespace std;

namespace openshot
{

	/**
	 * @brief This struct holds the location of a frame within a chunk.
	 *
	 * Chunks are small video files, which typically contain a few seconds of video each.
	 * Locating a frame among these small video files is accomplished by using
	 * this struct.
	 */
	struct ChunkLocation
	{
		int number; ///< The chunk number
		int frame; ///< The frame number
	};

	/**
	 * @brief This enumeration allows the user to choose which version
	 * of the chunk they would like (low, medium, or high quality).
	 *
	 * Since chunks contain multiple video streams, this version enumeration
	 * allows the user to choose which version of the chunk they would like.
	 * For example, if you want a small version with reduced quality, you can
	 * choose the THUMBNAIL version. This is used on the ChunkReader
	 * constructor.
	 */
	enum ChunkVersion
	{
		THUMBNAIL,	///< The lowest quality stream contained in this chunk file
		PREVIEW,	///< The medium quality stream contained in this chunk file
		FINAL		///< The highest quality stream contained in this chunk file
	};

	/**
	 * @brief This class reads a special chunk-formatted file, which can be easily
	 * shared in a distributed environment.
	 *
	 * It stores the video in small "chunks", which are really just short video clips,
	 * a few seconds each. A ChunkReader only needs the part of the chunk that contains
	 * the frames it is looking for. For example, if you only need the end of a video,
	 * only the last few chunks might be needed to successfully access those openshot::Frame objects.
	 *
	 * \code
	 * // This example demonstrates how to read a chunk folder and access frame objects inside it.
	 * ChunkReader r("/home/jonathan/apps/chunks/chunk1/", FINAL); // Load highest quality version of this chunk file
	 * r.DisplayInfo(); // Display all known details about this chunk file
	 * r.Open(); // Open the reader
	 *
	 * // Access frame 1
	 * r.GetFrame(1)->Display();
	 *
	 * // Close the reader
	 * r.Close();
	 * \endcode
	 */
	class ChunkReader : public ReaderBase
	{
	private:
		string path;
		bool is_open;
		int chunk_size;
		FFmpegReader *local_reader;
		ChunkLocation previous_location;
		ChunkVersion version;
		std::shared_ptr<Frame> last_frame;

		/// Check if folder path existing
		bool does_folder_exist(string path);

		/// Find the location of a frame in a chunk
		ChunkLocation find_chunk_frame(long int requested_frame);

		/// get a formatted path of a specific chunk
		string get_chunk_path(int chunk_number, string folder, string extension);

		/// Load JSON meta data about this chunk folder
		void load_json();

	public:

		/// @brief Constructor for ChunkReader.  This automatically opens the chunk file or folder and loads
		/// frame 1, or it throws one of the following exceptions.
		/// @param path				The folder path / location of a chunk (chunks are stored as folders)
		/// @param chunk_version	Choose the video version / quality (THUMBNAIL, PREVIEW, or FINAL)
		ChunkReader(string path, ChunkVersion chunk_version) throw(InvalidFile, InvalidJSON);

		/// Close the reader
		void Close();

		/// @brief Get the chunk size (number of frames to write in each chunk)
		/// @returns	The number of frames in this chunk
		int GetChunkSize() { return chunk_size; };

		/// @brief Set the chunk size (number of frames to write in each chunk)
		/// @param new_size		The number of frames per chunk
		void SetChunkSize(int new_size) { chunk_size = new_size; };

		/// Get the cache object used by this reader (always return NULL for this reader)
		CacheMemory* GetCache() { return NULL; };

		/// @brief Get an openshot::Frame object for a specific frame number of this reader.
		/// @returns				The requested frame (containing the image and audio)
		/// @param requested_frame	The frame number you want to retrieve
		std::shared_ptr<Frame> GetFrame(long int requested_frame) throw(ReaderClosed, ChunkNotFound);

		/// Determine if reader is open or closed
		bool IsOpen() { return is_open; };

		/// Return the type name of the class
		string Name() { return "ChunkReader"; };

		/// Get and Set JSON methods
		string Json(); ///< Generate JSON string of this object
		void SetJson(string value) throw(InvalidJSON); ///< Load JSON string into this object
		Json::Value JsonValue(); ///< Generate Json::JsonValue for this object
		void SetJsonValue(Json::Value root) throw(InvalidFile); ///< Load Json::JsonValue into this object

		/// Open the reader. This is required before you can access frames or data from the reader.
		void Open() throw(InvalidFile);
	};

}

#endif
