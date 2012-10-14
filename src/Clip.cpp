#include "../include/Clip.h"

using namespace openshot;

// Init default settings for a clip
void Clip::init_settings()
{
	// Init clip settings
	Position(0.0);
	Layer(0);
	Start(0.0);
	End(0.0);
	gravity = GRAVITY_CENTER;
	scale = SCALE_FIT;
	anchor = ANCHOR_CANVAS;

	// Init scale curves
	scale_x = Keyframe(100.0);
	scale_y = Keyframe(100.0);

	// Init location curves
	location_x = Keyframe(0.0);
	location_y = Keyframe(0.0);

	// Init alpha & rotation
	alpha = Keyframe(100.0);
	rotation = Keyframe(0.0);

	// Init time & volume
	time = Keyframe(0.0);
	volume = Keyframe(100.0);

	// Init crop settings
	crop_gravity = GRAVITY_CENTER;
	crop_width = Keyframe(-1.0);
	crop_height = Keyframe(-1.0);
	crop_x = Keyframe(0.0);
	crop_y = Keyframe(0.0);

	// Init shear and perspective curves
	shear_x = Keyframe(0.0);
	shear_y = Keyframe(0.0);
	perspective_c1_x = Keyframe(-1.0);
	perspective_c1_y = Keyframe(-1.0);
	perspective_c2_x = Keyframe(-1.0);
	perspective_c2_y = Keyframe(-1.0);
	perspective_c3_x = Keyframe(-1.0);
	perspective_c3_y = Keyframe(-1.0);
	perspective_c4_x = Keyframe(-1.0);
	perspective_c4_y = Keyframe(-1.0);

	// Default pointers
	frame_map = NULL;
	file_reader = NULL;
}

// Default Constructor for a clip
Clip::Clip()
{
	// Init all default settings
	init_settings();
}

// Constructor with reader
Clip::Clip(FileReaderBase* reader)
{
	// set reader pointer
	file_reader = reader;
}

// Constructor with filepath
Clip::Clip(string path)
{
	// Init all default settings
	init_settings();

	// Get file extension (and convert to lower case)
	string ext = get_file_extension(path);
	transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	// Determine if common video formats
	if (ext=="avi" || ext=="mov" || ext=="mkv" ||  ext=="mpg" || ext=="mpeg" || ext=="mp3" || ext=="mp4" || ext=="mts" ||
		ext=="ogg" || ext=="wav" || ext=="wmv" || ext=="webm" || ext=="vob")
	{
		try
		{
			// Open common video format
			file_reader = new FFmpegReader(path);
			cout << "READER FOUND: FFmpegReader" << endl;
		} catch(...) { }
	}

	// If no video found, try each reader
	if (!file_reader)
	{
		try
		{
			// Try an image reader
			file_reader = new ImageReader(path);
			cout << "READER FOUND: ImageReader" << endl;

		} catch(...) {
			try
			{
				// Try a video reader
				file_reader = new FFmpegReader(path);
				cout << "READER FOUND: FFmpegReader" << endl;

			} catch(BaseException ex) {
				// No Reader Found, Throw an exception
				cout << "READER NOT FOUND" << endl;
				throw ex;
			}
		}
	}

}

/// Set the current reader
void Clip::Reader(FileReaderBase* reader)
{
	// set reader pointer
	file_reader = reader;
}

/// Get the current reader
FileReaderBase* Clip::Reader()
{
	return file_reader;
}

// Open the internal reader
void Clip::Open() throw(InvalidFile)
{
	if (file_reader)
	{
		// Open the reader
		file_reader->Open();

		// Set some clip properties from the file reader
		End(file_reader->info.duration);
	}
}

// Close the internal reader
void Clip::Close()
{
	if (file_reader)
		file_reader->Close();
}

// Get end position of clip (trim end of video), which can be affected by the time curve.
float Clip::End()
{
	// Determine the FPS fo this clip
	float fps = 24.0;
	if (frame_map)
		// frame mapper
		fps = frame_map->TargetFPS().GetFPS();
	else if (file_reader)
		// file reader
		fps = file_reader->info.fps.ToFloat();

	// if a time curve is present, use it's length
	if (time.Points.size() > 1)
		return float(time.Values.size()) / fps;
	else
		// just use the duration (as detected by the reader)
		return end;
}

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> Clip::GetFrame(int requested_frame) throw(ReaderClosed)
{
	// Adjust out of bounds frame number
	requested_frame = adjust_frame_number_minimum(requested_frame);

	// Get mapped frame number (or just return the same number).  This is used to change framerates.
	int frame_number = get_framerate_mapped_frame(requested_frame);

	// Get time mapped frame number (used to increase speed, change direction, etc...)
	frame_number = adjust_frame_number_minimum(get_time_mapped_frame(frame_number));

	// Now that we have re-mapped what frame number is needed, go and get the frame pointer
	tr1::shared_ptr<Frame> frame = file_reader->GetFrame(frame_number);

	// Apply basic image processing (scale, rotation, etc...)
	apply_basic_image_processing(frame, frame_number);

	// Return processed 'frame'
	return frame;
}

// Map frame rate of this clip to a different frame rate
void Clip::MapFrames(Framerate fps, Pulldown_Method pulldown)
{
	// Check for a valid file reader (required to re-map it's frame rate)
	if (file_reader)
	{
		// Get original framerate
		Framerate original_fps(file_reader->info.fps.num, file_reader->info.fps.den);

		// Create and Set FrameMapper object
		frame_map = new FrameMapper(file_reader->info.video_length, original_fps, fps, pulldown);
	}
}

// Get file extension
string Clip::get_file_extension(string path)
{
	// return last part of path
	return path.substr(path.find_last_of(".") + 1);
}

// Get the new frame number, based on the Framemapper, or return the number passed in
int Clip::get_framerate_mapped_frame(int original_frame_number)
{
	// Check for a frame mapper (which is optinal)
	if (frame_map)
	{
		// Get new frame number
		return frame_map->GetFrame(original_frame_number).Odd.Frame;

	} else
		// return passed in parameter
		return original_frame_number;
}

// Get the new frame number, based on a time map curve (used to increase speed, change direction, etc...)
int Clip::get_time_mapped_frame(int original_frame_number)
{
	// Check for a valid time map curve
	if (time.Values.size() > 1)
	{
		// Get new frame number
		return time.GetValue(original_frame_number);

	} else
		// return passed in parameter
		return original_frame_number;
}

// Apply basic image processing (scale, rotate, move, etc...)
void Clip::apply_basic_image_processing(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Get values
	float rotation_value = rotation.GetValue(frame_number);
	//float scale_x_value = scale_x.GetValue(frame_number);
	//float scale_y_value = scale_y.GetValue(frame_number);

	// rotate frame
	if (rotation_value != 0)
		frame->Rotate(rotation_value);
}

// Adjust frame number minimum value
int Clip::adjust_frame_number_minimum(int frame_number)
{
	// Never return a frame number 0 or below
	if (frame_number < 1)
		return 1;
	else
		return frame_number;

}
