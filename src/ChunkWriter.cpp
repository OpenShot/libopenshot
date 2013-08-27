/**
 * \file
 * \brief Source code for the ChunkWriter class
 * \author Copyright (c) 2013 Jonathan Thomas
 */


#include "../include/ChunkWriter.h"

using namespace openshot;

ChunkWriter::ChunkWriter(string path, FileReaderBase *reader) throw (InvalidFile, InvalidFormat, InvalidCodec, InvalidOptions, OutOfMemory) :
		local_reader(reader), path(path), cache_size(8), is_writing(false)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Copy info struct from the source reader
	CopyReaderInfo(local_reader);

	// Create folder (if it does not exist)
	create_folder(path);

	// Write JSON meta data file
	write_json_meta_data();
}

// Add a frame to the queue waiting to be encoded.
void ChunkWriter::WriteFrame(tr1::shared_ptr<Frame> frame)
{
	// Add frame pointer to "queue", waiting to be processed the next
	// time the WriteFrames() method is called.
	spooled_frames.push_back(frame);

	// Write the frames once it reaches the correct cache size
	if (spooled_frames.size() == cache_size)
	{
		// Is writer currently writing?
		if (!is_writing)
			// Write frames to video file
			write_queued_frames();

		else
		{
			// YES, WRITING... so wait until it finishes, before writing again
			while (is_writing)
				Sleep(1); // sleep for 250 milliseconds

			// Write frames to video file
			write_queued_frames();
		}
	}

	// Keep track of the last frame added
	last_frame = frame;
}

// Write all frames in the queue to the video file.
void ChunkWriter::write_queued_frames()
{
	// Flip writing flag
	is_writing = true;

	// Transfer spool to queue
	queued_frames = spooled_frames;

	// Empty spool
	spooled_frames.clear();

	//omp_set_num_threads(1);
	omp_set_nested(true);
	#pragma omp parallel
	{
		#pragma omp single
		{
			// Loop through each queued image frame
			while (!queued_frames.empty())
			{
				// Get front frame (from the queue)
				tr1::shared_ptr<Frame> frame = queued_frames.front();

				// Add to processed queue
				processed_frames.push_back(frame);

				// Encode and add the frame to the output file
				process_frame(frame);

				// Remove front item
				queued_frames.pop_front();

			} // end while

			// Done writing
			is_writing = false;

		} // end omp single
	} // end omp parallel

}

// Write a block of frames from a reader
void ChunkWriter::WriteFrame(FileReaderBase* reader, int start, int length)
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
	// Reset frame counters
	write_video_count = 0;
	write_audio_count = 0;
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
	root["vcodec"] = local_reader->info.vcodec;
	stringstream video_length_stream;
	video_length_stream << local_reader->info.video_length;
	root["video_length"] = video_length_stream.str();
	root["video_stream_index"] = local_reader->info.video_stream_index;
	root["video_timebase"] = Json::Value(Json::objectValue);
	root["video_timebase"]["num"] = local_reader->info.video_timebase.num;
	root["video_timebase"]["den"] = local_reader->info.video_timebase.den;
	root["interlaced_frame"] = local_reader->info.interlaced_frame;
	root["top_field_first"] = local_reader->info.top_field_first;
	root["acodec"] = local_reader->info.acodec;
	root["audio_bit_rate"] = local_reader->info.audio_bit_rate;
	root["sample_rate"] = local_reader->info.sample_rate;
	root["channels"] = local_reader->info.channels;
	root["audio_stream_index"] = local_reader->info.audio_stream_index;
	root["audio_timebase"] = Json::Value(Json::objectValue);
	root["audio_timebase"]["num"] = local_reader->info.audio_timebase.num;
	root["audio_timebase"]["den"] = local_reader->info.audio_timebase.den;

	cout << root << endl;
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

// process frame
void ChunkWriter::process_frame(tr1::shared_ptr<Frame> frame)
{
	#pragma omp task firstprivate(frame)
	{
		// Determine the height & width of the source image
		int source_image_width = frame->GetWidth();
		int source_image_height = frame->GetHeight();

		// Generate frame image name
		stringstream thumb_name;
		stringstream preview_name;
		stringstream final_name;
		thumb_name << frame->number << "_t.JPG";
		preview_name << frame->number << "_p.JPG";
		final_name << frame->number << "_f.JPG";

		#pragma omp critical (chunk_output)
		cout << "Writing " << thumb_name.str() << endl;

		// Do nothing if size is 1x1 (i.e. no image in this frame)
		if (source_image_height > 1 && source_image_width > 1)
		{
			// Write image of frame to chunk
			frame->Save(thumb_name.str(), 0.25);
			frame->Save(preview_name.str(), 0.5);
			frame->Save(final_name.str(), 1.0);
		}



	} // end task

}



