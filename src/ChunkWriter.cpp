/**
 * @file
 * @brief Source file for ChunkWriter class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ChunkWriter.h"
#include "Exceptions.h"
#include "Frame.h"

using namespace openshot;

ChunkWriter::ChunkWriter(std::string path, ReaderBase *reader) :
		local_reader(reader), path(path), chunk_size(24*3), chunk_count(1), frame_count(1), is_writing(false),
		default_extension(".webm"), default_vcodec("libvpx"), default_acodec("libvorbis"), last_frame_needed(false), is_open(false)
{
	// Change codecs to default
	info.vcodec = default_vcodec;
	info.acodec = default_acodec;

	// Copy info struct from the source reader
	CopyReaderInfo(local_reader);

	// Create folder (if it does not exist)
	create_folder(path);

	// Write JSON meta data file
	write_json_meta_data();

	// Open reader
	local_reader->Open();
}

// get a formatted path of a specific chunk
std::string ChunkWriter::get_chunk_path(int64_t chunk_number, std::string folder, std::string extension)
{
	// Create path of new chunk video
	std::stringstream chunk_count_string;
	chunk_count_string << chunk_number;
	QString padded_count = "%1"; //chunk_count_string.str().c_str();
	padded_count = padded_count.arg(chunk_count_string.str().c_str(), 6, '0');
	if (folder.length() != 0 && extension.length() != 0)
		// Return path with FOLDER and EXTENSION name
		return QDir::cleanPath(QString(path.c_str()) + QDir::separator() + folder.c_str() + QDir::separator() + padded_count + extension.c_str()).toStdString();

	else if (folder.length() == 0 && extension.length() != 0)
		// Return path with NO FOLDER and EXTENSION name
		return QDir::cleanPath(QString(path.c_str()) + QDir::separator() + padded_count + extension.c_str()).toStdString();

	else if (folder.length() != 0 && extension.length() == 0)
		// Return path with FOLDER and NO EXTENSION
		return QDir::cleanPath(QString(path.c_str()) + QDir::separator() + folder.c_str()).toStdString();
	else
		return "";
}

// Add a frame to the queue waiting to be encoded.
void ChunkWriter::WriteFrame(std::shared_ptr<openshot::Frame> frame)
{
	// Check for open reader (or throw exception)
	if (!is_open)
		throw WriterClosed("The ChunkWriter is closed.  Call Open() before calling this method.", path);

	// Check if currently writing chunks?
	if (!is_writing)
	{
		// Save thumbnail of chunk start frame
		frame->Save(get_chunk_path(chunk_count, "", ".jpeg"), 1.0);

		// Create FFmpegWriter (FINAL quality)
		create_folder(get_chunk_path(chunk_count, "final", ""));
		writer_final = new FFmpegWriter(get_chunk_path(chunk_count, "final", default_extension));
		writer_final->SetAudioOptions(true, default_acodec, info.sample_rate, info.channels, info.channel_layout, 128000);
		writer_final->SetVideoOptions(true, default_vcodec, info.fps, info.width, info.height, info.pixel_ratio, false, false, info.video_bit_rate);

		// Create FFmpegWriter (PREVIEW quality)
		create_folder(get_chunk_path(chunk_count, "preview", ""));
		writer_preview = new FFmpegWriter(get_chunk_path(chunk_count, "preview", default_extension));
		writer_preview->SetAudioOptions(true, default_acodec, info.sample_rate, info.channels, info.channel_layout, 128000);
		writer_preview->SetVideoOptions(true, default_vcodec, info.fps, info.width * 0.5, info.height * 0.5, info.pixel_ratio, false, false, info.video_bit_rate * 0.5);

		// Create FFmpegWriter (LOW quality)
		create_folder(get_chunk_path(chunk_count, "thumb", ""));
		writer_thumb = new FFmpegWriter(get_chunk_path(chunk_count, "thumb", default_extension));
		writer_thumb->SetAudioOptions(true, default_acodec, info.sample_rate, info.channels, info.channel_layout, 128000);
		writer_thumb->SetVideoOptions(true, default_vcodec, info.fps, info.width * 0.25, info.height * 0.25, info.pixel_ratio, false, false, info.video_bit_rate * 0.25);

		// Prepare Streams
		writer_final->PrepareStreams();
		writer_preview->PrepareStreams();
		writer_thumb->PrepareStreams();

		// Write header
		writer_final->WriteHeader();
		writer_preview->WriteHeader();
		writer_thumb->WriteHeader();

		// Keep track that a chunk is being written
		is_writing = true;
		last_frame_needed = true;
	}

	// If this is not the 1st chunk, always start frame 1 with the last frame from the previous
	// chunk. This helps to prevent audio resampling issues (because it "stokes" the sample array)
	if (last_frame_needed)
	{
		if (last_frame)
		{
			// Write the previous chunks LAST FRAME to the current chunk
			writer_final->WriteFrame(last_frame);
			writer_preview->WriteFrame(last_frame);
			writer_thumb->WriteFrame(last_frame);
		} else {
			// Write the 1st frame (of the 1st chunk)... since no previous chunk is available
			auto blank_frame = std::make_shared<Frame>(
				1, info.width, info.height, "#000000",
				info.sample_rate, info.channels);
			blank_frame->AddColor(info.width, info.height, "#000000");
			writer_final->WriteFrame(blank_frame);
			writer_preview->WriteFrame(blank_frame);
			writer_thumb->WriteFrame(blank_frame);
		}

		// disable last frame
		last_frame_needed = false;
	}


	//////////////////////////////////////////////////
	// WRITE THE CURRENT FRAME TO THE CURRENT CHUNK
	writer_final->WriteFrame(frame);
	writer_preview->WriteFrame(frame);
	writer_thumb->WriteFrame(frame);
	//////////////////////////////////////////////////


	// Write the frames once it reaches the correct chunk size
	if (frame_count % chunk_size == 0 && frame_count >= chunk_size)
	{
		// Pad an additional 12 frames
		for (int z = 0; z<12; z++)
		{
			// Repeat frame
			writer_final->WriteFrame(frame);
			writer_preview->WriteFrame(frame);
			writer_thumb->WriteFrame(frame);
		}

		// Write Footer
		writer_final->WriteTrailer();
		writer_preview->WriteTrailer();
		writer_thumb->WriteTrailer();

		// Close writer & reader
		writer_final->Close();
		writer_preview->Close();
		writer_thumb->Close();

		// Increment chunk count
		chunk_count++;

		// Stop writing chunk
		is_writing = false;
	}

	// Increment frame counter
	frame_count++;

	// Keep track of the last frame added
	last_frame = frame;
}


// Write a block of frames from a reader
void ChunkWriter::WriteFrame(ReaderBase* reader, int64_t start, int64_t length)
{
	// Loop through each frame (and encoded it)
	for (int64_t number = start; number <= length; number++)
	{
		// Get the frame
		std::shared_ptr<Frame> f = reader->GetFrame(number);

		// Encode frame
		WriteFrame(f);
	}
}

// Write a block of frames from the local cached reader
void ChunkWriter::WriteFrame(int64_t start, int64_t length)
{
	// Loop through each frame (and encoded it)
	for (int64_t number = start; number <= length; number++)
	{
		// Get the frame
		std::shared_ptr<Frame> f = local_reader->GetFrame(number);

		// Encode frame
		WriteFrame(f);
	}
}

// Close the writer
void ChunkWriter::Close()
{
	// Write the frames once it reaches the correct chunk size
	if (is_writing)
	{
		// Pad an additional 12 frames
		for (int z = 0; z<12; z++)
		{
			// Repeat frame
			writer_final->WriteFrame(last_frame);
			writer_preview->WriteFrame(last_frame);
			writer_thumb->WriteFrame(last_frame);
		}

		// Write Footer
		writer_final->WriteTrailer();
		writer_preview->WriteTrailer();
		writer_thumb->WriteTrailer();

		// Close writer & reader
		writer_final->Close();
		writer_preview->Close();
		writer_thumb->Close();

		// Increment chunk count
		chunk_count++;

		// Stop writing chunk
		is_writing = false;
	}

	// close writer
	is_open = false;

	// Reset frame counters
	chunk_count = 0;
	frame_count = 0;

	// Open reader
	local_reader->Close();
}

// write JSON meta data
void ChunkWriter::write_json_meta_data()
{
	// Load path of chunk folder
	std::string json_path = QDir::cleanPath(QString(path.c_str()) + QDir::separator() + "info.json").toStdString();

	// Write JSON file
	std::ofstream myfile;
	myfile.open (json_path.c_str());
	myfile << local_reader->Json() << std::endl;
	myfile.close();
}

// check for chunk folder
void ChunkWriter::create_folder(std::string path)
{
	QDir dir(path.c_str());
	if (!dir.exists()) {
	    dir.mkpath(".");
	}
}

// check for valid chunk json
bool ChunkWriter::is_chunk_valid()
{
	return true;
}

// Open the writer
void ChunkWriter::Open()
{
	is_open = true;
}
