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
	gravity = CENTER;
	scale = FIT;
	anchor = CANVAS;

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
	crop_gravity = CENTER;
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
}

// Default Constructor for a clip
Clip::Clip()
{
	// Init all default settings
	init_settings();
}

// Constructor with filepath
Clip::Clip(string path)
{
	// Init all default settings
	init_settings();

	// Try each type of reader (until one works)
	try
	{
		file_reader = new ImageReader(path);
		cout << "READER FOUND: ImageReader" << endl;

	} catch(...) {
		try
		{
			file_reader = new FFmpegReader(path);
			cout << "READER FOUND: FFmpegReader" << endl;

		} catch(BaseException ex) {
			// let exception bubble up
			cout << "READER NOT FOUND" << endl;
			throw ex;
		}
	}

}
