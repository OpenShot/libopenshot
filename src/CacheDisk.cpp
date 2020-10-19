/**
 * @file
 * @brief Source file for CacheDisk class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
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

#include "CacheDisk.h"
#include "QtUtilities.h"
#include <Qt>
#include <QString>
#include <QTextStream>

using namespace std;
using namespace openshot;

// Default constructor, no max bytes
CacheDisk::CacheDisk(std::string cache_path, std::string format, float quality, float scale) : CacheBase(0) {
	// Set cache type name
	cache_type = "CacheDisk";
	range_version = 0;
	needs_range_processing = false;
	frame_size_bytes = 0;
	image_format = format;
	image_quality = quality;
	image_scale = scale;
	max_bytes = 0;

	// Init path directory
	InitPath(cache_path);
}

// Constructor that sets the max bytes to cache
CacheDisk::CacheDisk(std::string cache_path, std::string format, float quality, float scale, int64_t max_bytes) : CacheBase(max_bytes) {
	// Set cache type name
	cache_type = "CacheDisk";
	range_version = 0;
	needs_range_processing = false;
	frame_size_bytes = 0;
	image_format = format;
	image_quality = quality;
	image_scale = scale;

	// Init path directory
	InitPath(cache_path);
}

// Initialize cache directory
void CacheDisk::InitPath(std::string cache_path) {
	QString qpath;

	if (!cache_path.empty()) {
		// Init QDir with cache directory
		qpath = QString(cache_path.c_str());

	} else {
		// Init QDir with user's temp directory
		qpath = QDir::tempPath() + QString("/preview-cache/");
	}

	// Init QDir with cache directory
	path = QDir(qpath);

	// Check if cache directory exists
	if (!path.exists())
		// Create
		path.mkpath(qpath);
}

// Calculate ranges of frames
void CacheDisk::CalculateRanges() {
	// Only calculate when something has changed
	if (needs_range_processing) {

		// Create a scoped lock, to protect the cache from multiple threads
		const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

		// Sort ordered frame #s, and calculate JSON ranges
		std::sort(ordered_frame_numbers.begin(), ordered_frame_numbers.end());

		// Clear existing JSON variable
		Json::Value ranges = Json::Value(Json::arrayValue);

		// Increment range version
		range_version++;

		int64_t starting_frame = *ordered_frame_numbers.begin();
		int64_t ending_frame = starting_frame;

		// Loop through all known frames (in sequential order)
		for (const auto frame_number : ordered_frame_numbers) {
			if (frame_number - ending_frame > 1) {
				// End of range detected
				Json::Value range;

				// Add JSON object with start/end attributes
				// Use strings, since int64_ts are supported in JSON
				range["start"] = std::to_string(starting_frame);
				range["end"] = std::to_string(ending_frame);
				ranges.append(range);

				// Set new starting range
				starting_frame = frame_number;
			}

			// Set current frame as end of range, and keep looping
			ending_frame = frame_number;
		}

		// APPEND FINAL VALUE
		Json::Value range;

		// Add JSON object with start/end attributes
		// Use strings, since int64_ts are supported in JSON
		range["start"] = std::to_string(starting_frame);
		range["end"] = std::to_string(ending_frame);
		ranges.append(range);

		// Cache range JSON as string
		json_ranges = ranges.toStyledString();

		// Reset needs_range_processing
		needs_range_processing = false;
	}
}

// Default destructor
CacheDisk::~CacheDisk()
{
	frames.clear();
	frame_numbers.clear();
	ordered_frame_numbers.clear();

	// remove critical section
	delete cacheCriticalSection;
	cacheCriticalSection = NULL;
}

// Add a Frame to the cache
void CacheDisk::Add(std::shared_ptr<Frame> frame)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);
	int64_t frame_number = frame->number;

	// Freshen frame if it already exists
	if (frames.count(frame_number))
		// Move frame to front of queue
		MoveToFront(frame_number);

	else
	{
		// Add frame to queue and map
		frames[frame_number] = frame_number;
		frame_numbers.push_front(frame_number);
		ordered_frame_numbers.push_back(frame_number);
		needs_range_processing = true;

		// Save image to disk (if needed)
		QString frame_path(path.path() + "/" + QString("%1.").arg(frame_number) + QString(image_format.c_str()).toLower());
		frame->Save(frame_path.toStdString(), image_scale, image_format, image_quality);
		if (frame_size_bytes == 0) {
			// Get compressed size of frame image (to correctly apply max size against)
			QFile image_file(frame_path);
			frame_size_bytes = image_file.size();
		}

		// Save audio data (if needed)
		if (frame->has_audio_data) {
			QString audio_path(path.path() + "/" + QString("%1").arg(frame_number) + ".audio");
			QFile audio_file(audio_path);

			if (audio_file.open(QIODevice::WriteOnly)) {
				QTextStream audio_stream(&audio_file);
				audio_stream << frame->SampleRate() << Qt::endl;
				audio_stream << frame->GetAudioChannelsCount() << Qt::endl;
				audio_stream << frame->GetAudioSamplesCount() << Qt::endl;
				audio_stream << frame->ChannelsLayout() << Qt::endl;

				// Loop through all samples
				for (int channel = 0; channel < frame->GetAudioChannelsCount(); channel++)
				{
					// Get audio for this channel
					float *samples = frame->GetAudioSamples(channel);
					for (int sample = 0; sample < frame->GetAudioSamplesCount(); sample++)
						audio_stream << samples[sample] << Qt::endl;
				}

			}

		}

		// Clean up old frames
		CleanUp();
	}
}

// Get a frame from the cache (or NULL shared_ptr if no frame is found)
std::shared_ptr<Frame> CacheDisk::GetFrame(int64_t frame_number)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Does frame exists in cache?
	if (frames.count(frame_number)) {
		// Does frame exist on disk
		QString frame_path(path.path() + "/" + QString("%1.").arg(frame_number) + QString(image_format.c_str()).toLower());
		if (path.exists(frame_path)) {

			// Load image file
			auto image = std::make_shared<QImage>();
			image->load(frame_path);

			// Set pixel formatimage->
			image = std::make_shared<QImage>(image->convertToFormat(QImage::Format_RGBA8888));

			// Create frame object
			auto frame = std::make_shared<Frame>();
			frame->number = frame_number;
			frame->AddImage(image);

			// Get audio data (if found)
			QString audio_path(path.path() + "/" + QString("%1").arg(frame_number) + ".audio");
			QFile audio_file(audio_path);
			if (audio_file.exists()) {
				// Open audio file
				QTextStream in(&audio_file);
				if (audio_file.open(QIODevice::ReadOnly)) {
					int sample_rate = in.readLine().toInt();
					int channels = in.readLine().toInt();
					int sample_count = in.readLine().toInt();
					int channel_layout = in.readLine().toInt();

					// Set basic audio properties
					frame->ResizeAudio(channels, sample_count, sample_rate, (ChannelLayout) channel_layout);

					// Loop through audio samples and add to frame
					int current_channel = 0;
					int current_sample = 0;
					float *channel_samples = new float[sample_count];
					while (!in.atEnd()) {
						// Add sample to channel array
						channel_samples[current_sample] = in.readLine().toFloat();
						current_sample++;

						if (current_sample == sample_count) {
							// Add audio to frame
							frame->AddAudio(true, current_channel, 0, channel_samples, sample_count, 1.0);

							// Increment channel, and reset sample position
							current_channel++;
							current_sample = 0;
						}

					}
				}
			}

			// return the Frame object
			return frame;
		}
	}

	// no Frame found
	return std::shared_ptr<Frame>();
}

// Get the smallest frame number (or NULL shared_ptr if no frame is found)
std::shared_ptr<Frame> CacheDisk::GetSmallestFrame()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);
	std::shared_ptr<openshot::Frame> f;

	// Loop through frame numbers
	std::deque<int64_t>::iterator itr;
	int64_t smallest_frame = -1;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
	{
		if (*itr < smallest_frame || smallest_frame == -1)
			smallest_frame = *itr;
	}

	// Return frame
	f = GetFrame(smallest_frame);

	return f;
}

// Gets the maximum bytes value
int64_t CacheDisk::GetBytes()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	int64_t  total_bytes = 0;

	// Loop through frames, and calculate total bytes
	std::deque<int64_t>::reverse_iterator itr;
	for(itr = frame_numbers.rbegin(); itr != frame_numbers.rend(); ++itr)
		total_bytes += frame_size_bytes;

	return total_bytes;
}

// Remove a specific frame
void CacheDisk::Remove(int64_t frame_number)
{
	Remove(frame_number, frame_number);
}

// Remove range of frames
void CacheDisk::Remove(int64_t start_frame_number, int64_t end_frame_number)
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Loop through frame numbers
	std::deque<int64_t>::iterator itr;
	for(itr = frame_numbers.begin(); itr != frame_numbers.end();)
	{
		//deque<int64_t>::iterator current = itr++;
		if (*itr >= start_frame_number && *itr <= end_frame_number)
		{
			// erase frame number
			itr = frame_numbers.erase(itr);
		} else
			itr++;
	}

	// Loop through ordered frame numbers
	std::vector<int64_t>::iterator itr_ordered;
	for(itr_ordered = ordered_frame_numbers.begin(); itr_ordered != ordered_frame_numbers.end();)
	{
		if (*itr_ordered >= start_frame_number && *itr_ordered <= end_frame_number)
		{
			// erase frame number
			frames.erase(*itr_ordered);

			// Remove the image file (if it exists)
			QString frame_path(path.path() + "/" + QString("%1.").arg(*itr_ordered) + QString(image_format.c_str()).toLower());
			QFile image_file(frame_path);
			if (image_file.exists())
				image_file.remove();

			// Remove audio file (if it exists)
			QString audio_path(path.path() + "/" + QString("%1").arg(*itr_ordered) + ".audio");
			QFile audio_file(audio_path);
			if (audio_file.exists())
				audio_file.remove();

			itr_ordered = ordered_frame_numbers.erase(itr_ordered);
		} else
			itr_ordered++;
	}

	// Needs range processing (since cache has changed)
	needs_range_processing = true;
}

// Move frame to front of queue (so it lasts longer)
void CacheDisk::MoveToFront(int64_t frame_number)
{
	// Does frame exists in cache?
	if (frames.count(frame_number))
	{
		// Create a scoped lock, to protect the cache from multiple threads
		const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

		// Loop through frame numbers
		std::deque<int64_t>::iterator itr;
		for(itr = frame_numbers.begin(); itr != frame_numbers.end(); ++itr)
		{
			if (*itr == frame_number)
			{
				// erase frame number
				frame_numbers.erase(itr);

				// add frame number to 'front' of queue
				frame_numbers.push_front(frame_number);
				break;
			}
		}
	}
}

// Clear the cache of all frames
void CacheDisk::Clear()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Clear all containers
	frames.clear();
	frame_numbers.clear();
	ordered_frame_numbers.clear();
	needs_range_processing = true;
	frame_size_bytes = 0;

	// Delete cache directory, and recreate it
	QString current_path = path.path();
	path.removeRecursively();

	// Re-init folder
	InitPath(current_path.toStdString());
}

// Count the frames in the queue
int64_t CacheDisk::Count()
{
	// Create a scoped lock, to protect the cache from multiple threads
	const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

	// Return the number of frames in the cache
	return frames.size();
}

// Clean up cached frames that exceed the number in our max_bytes variable
void CacheDisk::CleanUp()
{
	// Do we auto clean up?
	if (max_bytes > 0)
	{
		// Create a scoped lock, to protect the cache from multiple threads
		const GenericScopedLock<CriticalSection> lock(*cacheCriticalSection);

		while (GetBytes() > max_bytes && frame_numbers.size() > 20)
		{
			// Get the oldest frame number.
			int64_t frame_to_remove = frame_numbers.back();

			// Remove frame_number and frame
			Remove(frame_to_remove);
		}
	}
}

// Generate JSON string of this object
std::string CacheDisk::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value CacheDisk::JsonValue() {

	// Process range data (if anything has changed)
	CalculateRanges();

	// Create root json object
	Json::Value root = CacheBase::JsonValue(); // get parent properties
	root["type"] = cache_type;
	root["path"] = path.path().toStdString();

	Json::Value version;
	std::stringstream range_version_str;
	range_version_str << range_version;
	root["version"] = range_version_str.str();

	// Parse and append range data (if any)
	// Parse and append range data (if any)
	try {
		const Json::Value ranges = openshot::stringToJson(json_ranges);
		root["ranges"] = ranges;
	} catch (...) { }

	// return JsonValue
	return root;
}

// Load JSON string into this object
void CacheDisk::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void CacheDisk::SetJsonValue(const Json::Value root) {

	// Close timeline before we do anything (this also removes all open and closing clips)
	Clear();

	// Set parent data
	CacheBase::SetJsonValue(root);

	if (!root["type"].isNull())
		cache_type = root["type"].asString();
	if (!root["path"].isNull())
		// Update duration of timeline
		InitPath(root["path"].asString());
}
