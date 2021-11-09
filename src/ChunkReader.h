/**
 * @file
 * @brief Header file for ChunkReader class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CHUNK_READER_H
#define OPENSHOT_CHUNK_READER_H

#include <string>
#include <memory>

#include "ReaderBase.h"
#include "Json.h"

namespace openshot
{
	class CacheBase;
	class Frame;
	/**
	 * @brief This struct holds the location of a frame within a chunk.
	 *
	 * Chunks are small video files, which typically contain a few seconds of video each.
	 * Locating a frame among these small video files is accomplished by using
	 * this struct.
	 */
	struct ChunkLocation
	{
		int64_t number; ///< The chunk number
		int64_t frame; ///< The frame number
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
		std::string path;
		bool is_open;
		int64_t chunk_size;
		openshot::ReaderBase *local_reader;
		ChunkLocation previous_location;
		ChunkVersion version;
		std::shared_ptr<openshot::Frame> last_frame;

		/// Check if folder path existing
		bool does_folder_exist(std::string path);

		/// Find the location of a frame in a chunk
		ChunkLocation find_chunk_frame(int64_t requested_frame);

		/// get a formatted path of a specific chunk
		std::string get_chunk_path(int64_t chunk_number, std::string folder, std::string extension);

		/// Load JSON meta data about this chunk folder
		void load_json();

	public:

		/// @brief Constructor for ChunkReader.  This automatically opens the chunk file or folder and loads
		/// frame 1, or it throws one of the following exceptions.
		/// @param path				The folder path / location of a chunk (chunks are stored as folders)
		/// @param chunk_version	Choose the video version / quality (THUMBNAIL, PREVIEW, or FINAL)
		ChunkReader(std::string path, ChunkVersion chunk_version);

		/// Close the reader
		void Close() override;

		/// @brief Get the chunk size (number of frames to write in each chunk)
		/// @returns	The number of frames in this chunk
		int64_t GetChunkSize() { return chunk_size; };

		/// @brief Set the chunk size (number of frames to write in each chunk)
		/// @param new_size		The number of frames per chunk
		void SetChunkSize(int64_t new_size) { chunk_size = new_size; };

		/// Get the cache object used by this reader (always return NULL for this reader)
		openshot::CacheBase* GetCache() override { return nullptr; };

		/// @brief Get an openshot::Frame object for a specific frame number of this reader.
		/// @returns				The requested frame (containing the image and audio)
		/// @param requested_frame	The frame number you want to retrieve
		std::shared_ptr<openshot::Frame> GetFrame(int64_t requested_frame) override;

		/// Determine if reader is open or closed
		bool IsOpen() override { return is_open; };

		/// Return the type name of the class
		std::string Name() override { return "ChunkReader"; };

		// Get and Set JSON methods
		std::string Json() const override; ///< Generate JSON string of this object
		void SetJson(const std::string value) override; ///< Load JSON string into this object
		Json::Value JsonValue() const override; ///< Generate Json::Value for this object
		void SetJsonValue(const Json::Value root) override; ///< Load Json::Value into this object

		/// Open the reader. This is required before you can access frames or data from the reader.
		void Open() override;
	};

}

#endif
