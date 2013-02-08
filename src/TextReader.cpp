#include "../include/TextReader.h"

using namespace openshot;

TextReader::TextReader(int width, int height, int x_offset, int y_offset, string text, string font, double size, string text_color, string background_color)
: width(width), height(height), x_offset(x_offset), y_offset(y_offset), text(text), font(font), size(size), text_color(text_color), background_color(background_color), is_open(false)
{
	// Init FileInfo struct (clear all values)
	InitFileInfo();

	// Open and Close the reader, to populate it's attributes (such as height, width, etc...)
	Open();
	Close();
}

// Open reader
void TextReader::Open()
{
	// Open reader if not already open
	if (!is_open)
	{
		// create image
		image = tr1::shared_ptr<Magick::Image>(new Magick::Image(Magick::Geometry(width,height), Magick::Color(background_color)));

		// Give image a transparent background color
		image->backgroundColor(Magick::Color("none"));

		// Set stroke properties
		lines.push_back(Magick::DrawableStrokeColor(Magick::Color("none")));
		lines.push_back(Magick::DrawableStrokeWidth(0.0));
		lines.push_back(Magick::DrawableFillColor(text_color));
		lines.push_back(Magick::DrawableFont(font));
		lines.push_back(Magick::DrawablePointSize(size));
		lines.push_back(Magick::DrawableText(x_offset, y_offset, text));

		// Draw image
		image->draw(lines);

		// Update image properties
		info.has_audio = false;
		info.has_video = true;
		info.file_size = image->fileSize();
		info.vcodec = image->format();
		info.width = image->size().width();
		info.height = image->size().height();
		info.pixel_ratio.num = 1;
		info.pixel_ratio.den = 1;
		info.duration = 60 * 60 * 24; // 24 hour duration
		info.fps.num = 30;
		info.fps.den = 1;
		info.video_timebase.num = 1;
		info.video_timebase.den = 30;
		info.video_length = round(info.duration * info.fps.ToDouble());

		// Calculate the DAR (display aspect ratio)
		Fraction size(info.width * info.pixel_ratio.num, info.height * info.pixel_ratio.den);

		// Reduce size fraction
		size.Reduce();

		// Set the ratio based on the reduced fraction
		info.display_ratio.num = size.num;
		info.display_ratio.den = size.den;

		// Mark as "open"
		is_open = true;
	}
}

// Close reader
void TextReader::Close()
{
	// Close all objects, if reader is 'open'
	if (is_open)
	{
		// Mark as "closed"
		is_open = false;
	}
}

// Get an openshot::Frame object for a specific frame number of this reader.
tr1::shared_ptr<Frame> TextReader::GetFrame(int requested_frame) throw(ReaderClosed)
{
	if (image)
	{
		// Create or get frame object
		tr1::shared_ptr<Frame> image_frame(new Frame(requested_frame, image->size().width(), image->size().height(), "#000000", 0, 2));
		image_frame->SetSampleRate(44100);

		// Add Image data to frame
		tr1::shared_ptr<Magick::Image> copy_image(new Magick::Image(*image.get()));
		copy_image->modifyImage(); // actually copy the image data to this object
		image_frame->AddImage(copy_image);

		// return frame object
		return image_frame;
	}

}


