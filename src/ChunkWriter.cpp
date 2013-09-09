/**
 * \file
 * \brief Source code for the ChunkWriter class
 * \author Copyright (c) 2008-2013 OpenShot Studios, LLC
 */


#include "../include/ChunkWriter.h"

using namespace openshot;

ChunkWriter::ChunkWriter(string path, ReaderBase *reader) throw (InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory) :
		local_reader(reader), path(path), chunk_size(24*3), chunk_count(1), frame_count(1), is_writing(false),
		default_extension(".webm"), default_vcodec("libvpx"), default_acodec("libvorbis")
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

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
string ChunkWriter::get_chunk_path(int chunk_number, string folder, string extension)
{
	// Create path of new chunk video
	stringstream chunk_count_string;
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
}

// Add a frame to the queue waiting to be encoded.
void ChunkWriter::WriteFrame(tr1::shared_ptr<Frame> frame)
{
	// Check if currently writing chunks?
	if (!is_writing)
	{
		// Save thumbnail of chunk start frame
		frame->Save(get_chunk_path(chunk_count, "", ".jpeg"), 1.0);

		// Create FFmpegWriter (FINAL quality)
		create_folder(get_chunk_path(chunk_count, "final", ""));
		writer_final = new FFmpegWriter(get_chunk_path(chunk_count, "final", default_extension));
		writer_final->SetAudioOptions(true, default_acodec, info.sample_rate, info.channels, 128000);
		writer_final->SetVideoOptions(true, default_vcodec, info.fps, info.width, info.height, info.pixel_ratio, false, false, info.video_bit_rate);

		// Create FFmpegWriter (PREVIEW quality)
		create_folder(get_chunk_path(chunk_count, "preview", ""));
		writer_preview = new FFmpegWriter(get_chunk_path(chunk_count, "preview", default_extension));
		writer_preview->SetAudioOptions(true, default_acodec, info.sample_rate, info.channels, 128000);
		writer_preview->SetVideoOptions(true, default_vcodec, info.fps, info.width * 0.5, info.height * 0.5, info.pixel_ratio, false, false, info.video_bit_rate * 0.5);

		// Create FFmpegWriter (LOW quality)
		create_folder(get_chunk_path(chunk_count, "thumb", ""));
		writer_thumb = new FFmpegWriter(get_chunk_path(chunk_count, "thumb", default_extension));
		writer_thumb->SetAudioOptions(true, default_acodec, info.sample_rate, info.channels, 128000);
		writer_thumb->SetVideoOptions(true, default_vcodec, info.fps, info.width * 0.25, info.height * 0.25, info.pixel_ratio, false, false, info.video_bit_rate * 0.25);

		// Prepare Streams
		writer_final->PrepareStreams();
		writer_preview->PrepareStreams();
		writer_thumb->PrepareStreams();

		// Write header
		writer_final->WriteHeader();
		writer_preview->WriteHeader();
		writer_thumb->WriteHeader();

		// Keep track that a chunk is being writen
		is_writing = true;
	}

	// Write a frame to the current chunk
	writer_final->WriteFrame(frame);
	writer_preview->WriteFrame(frame);
	writer_thumb->WriteFrame(frame);

	// Write the frames once it reaches the correct chunk size
	if (frame_count % chunk_size == 0 && frame_count >= chunk_size)
	{
		cout << "Done with chunk" << endl;
		cout << "frame_count: " << frame_count << endl;
		cout << "chunk_size: " << chunk_size << endl;

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
void ChunkWriter::WriteFrame(ReaderBase* reader, int start, int length)
{
	// Loop through each frame (and encoded it)
	for (int number = start; number <= length; number++)
	{
		// Get the frame
		tr1::shared_ptr<Frame> f = reader->GetFrame(number);

		// Encode frame
		WriteFrame(f);
	}
}

// Write a block of frames from the local cached reader
void ChunkWriter::WriteFrame(int start, int length)
{
	// Loop through each frame (and encoded it)
	for (int number = start; number <= length; number++)
	{
		// Get the frame
		tr1::shared_ptr<Frame> f = local_reader->GetFrame(number);

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
		cout << "Final chunk" << endl;
		cout << "frame_count: " << frame_count << endl;
		cout << "chunk_size: " << chunk_size << endl;

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

	// Reset frame counters
	chunk_count = 0;
	frame_count = 0;

	// Open reader
	local_reader->Close();
}

// write json meta data
void ChunkWriter::write_json_meta_data()
{
	Json::Value root;
	root["has_video"] = local_reader->info.has_video;
	root["has_audio"] = local_reader->info.has_audio;
	root["duration"] = local_reader->info.duration;
	stringstream filesize_stream;
	filesize_stream << local_reader->info.file_size;
	root["file_size"] = filesize_stream.str();
	root["height"] = local_reader->info.height;
	root["width"] = local_reader->info.width;
	root["pixel_format"] = local_reader->info.pixel_format;
	root["fps"] = Json::Value(Json::objectValue);
	root["fps"]["num"] = local_reader->info.fps.num;
	root["fps"]["den"] = local_reader->info.fps.den;
	root["video_bit_rate"] = local_reader->info.video_bit_rate;
	root["pixel_ratio"] = Json::Value(Json::objectValue);
	root["pixel_ratio"]["num"] = local_reader->info.pixel_ratio.num;
	root["pixel_ratio"]["den"] = local_reader->info.pixel_ratio.den;
	root["display_ratio"] = Json::Value(Json::objectValue);
	root["display_ratio"]["num"] = local_reader->info.display_ratio.num;
	root["display_ratio"]["den"] = local_reader->info.display_ratio.den;
	root["vcodec"] = default_vcodec;
	stringstream video_length_stream;
	video_length_stream << local_reader->info.video_length;
	root["video_length"] = video_length_stream.str();
	root["video_stream_index"] = local_reader->info.video_stream_index;
	root["video_timebase"] = Json::Value(Json::objectValue);
	root["video_timebase"]["num"] = local_reader->info.video_timebase.num;
	root["video_timebase"]["den"] = local_reader->info.video_timebase.den;
	root["interlaced_frame"] = local_reader->info.interlaced_frame;
	root["top_field_first"] = local_reader->info.top_field_first;
	root["acodec"] = default_acodec;
	root["audio_bit_rate"] = local_reader->info.audio_bit_rate;
	root["sample_rate"] = local_reader->info.sample_rate;
	root["channels"] = local_reader->info.channels;
	root["audio_stream_index"] = local_reader->info.audio_stream_index;
	root["audio_timebase"] = Json::Value(Json::objectValue);
	root["audio_timebase"]["num"] = local_reader->info.audio_timebase.num;
	root["audio_timebase"]["den"] = local_reader->info.audio_timebase.den;

	// Load path of chunk folder
	string json_path = QDir::cleanPath(QString(path.c_str()) + QDir::separator() + "info.json").toStdString();

	// Write JSON file
	ofstream myfile;
	myfile.open (json_path.c_str());
	myfile << root << endl;
	myfile.close();
}

// check for chunk folder
bool ChunkWriter::create_folder(string path)
{
	QDir dir(path.c_str());
	if (!dir.exists()) {
	    dir.mkpath(".");
	}
}

// check for valid chunk json
bool ChunkWriter::is_chunk_valid()
{

}


