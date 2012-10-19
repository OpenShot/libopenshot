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
	file_reader = NULL;
	resampler = NULL;
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
	if (file_reader)
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

	// Is a time map detected
	int new_frame_number = requested_frame;
	if (time.Values.size() > 1)
		new_frame_number = time.GetValue(requested_frame);

	// Now that we have re-mapped what frame number is needed, go and get the frame pointer
	tr1::shared_ptr<Frame> frame = file_reader->GetFrame(new_frame_number);

	// Get time mapped frame number (used to increase speed, change direction, etc...)
	apply_time_mapped_frame(frame, requested_frame);

	// Apply basic image processing (scale, rotation, etc...)
	apply_basic_image_processing(frame, new_frame_number);

	// Return processed 'frame'
	return frame;
}

// Get file extension
string Clip::get_file_extension(string path)
{
	// return last part of path
	return path.substr(path.find_last_of(".") + 1);
}

// Adjust the audio and image of a time mapped frame
void Clip::apply_time_mapped_frame(tr1::shared_ptr<Frame> frame, int frame_number)
{
	// Check for a valid time map curve
	if (time.Values.size() > 1)
	{
		// Create a resampler (only once)
		if (!resampler)
			resampler = new AudioResampler();

		// Get new frame number
		int new_frame_number = time.GetValue(frame_number);

		// Get previous and next values (if any)
		int previous_value = time.GetValue(frame_number - 1);
		int next_value = time.GetValue(frame_number + 1);

		// walk the curve (for 30 X coordinates), and try and detect direction
		bool reverse = false;
		for (int index = frame_number; index < frame_number + 30; index++)
		{
			if (time.GetValue(index) > new_frame_number)
			{
				// reverse time
				reverse = true;
				break;
			}
			else if (time.GetValue(index) < new_frame_number)
				// forward time
				break;
		}

		// difference between previous frame and current frame
		int previous_diff = abs(new_frame_number - previous_value);

		// Are we repeating frames or skipping frames?
		if ((previous_value == new_frame_number && frame_number > 0) ||
			(next_value == new_frame_number && frame_number < time.Values.size()))
		{
			// Slow down audio (to make it fit on more frames)



		}
		else if (previous_diff > 0 && previous_diff <= 30 && frame_number > 0)
		{
			// Speed up audio (to make it fit on less frames)


		}



	}
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
