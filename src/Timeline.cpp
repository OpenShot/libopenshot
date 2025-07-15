/**
 * @file
 * @brief Source file for Timeline class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Timeline.h"

#include "CacheBase.h"
#include "CacheDisk.h"
#include "CacheMemory.h"
#include "CrashHandler.h"
#include "FrameMapper.h"
#include "Exceptions.h"

#include <QDir>
#include <QFileInfo>

using namespace openshot;

// Default Constructor for the timeline (which sets the canvas width and height)
Timeline::Timeline(int width, int height, Fraction fps, int sample_rate, int channels, ChannelLayout channel_layout) :
		is_open(false), auto_map_clips(true), managed_cache(true), path(""),
		max_concurrent_frames(OPEN_MP_NUM_PROCESSORS), max_time(0.0)
{
	// Create CrashHandler and Attach (incase of errors)
	CrashHandler::Instance();

	// Init viewport size (curve based, because it can be animated)
	viewport_scale = Keyframe(100.0);
	viewport_x = Keyframe(0.0);
	viewport_y = Keyframe(0.0);

	// Init background color
	color.red = Keyframe(0.0);
	color.green = Keyframe(0.0);
	color.blue = Keyframe(0.0);

	// Init FileInfo struct (clear all values)
	info.width = width;
	info.height = height;
	preview_width = info.width;
	preview_height = info.height;
	info.fps = fps;
	info.sample_rate = sample_rate;
	info.channels = channels;
	info.channel_layout = channel_layout;
	info.video_timebase = fps.Reciprocal();
	info.duration = 60 * 30; // 30 minute default duration
	info.has_audio = true;
	info.has_video = true;
	info.video_length = info.fps.ToFloat() * info.duration;
	info.display_ratio = openshot::Fraction(width, height);
	info.display_ratio.Reduce();
	info.pixel_ratio = openshot::Fraction(1, 1);
	info.acodec = "openshot::timeline";
	info.vcodec = "openshot::timeline";

	// Init max image size
	SetMaxSize(info.width, info.height);

	// Init cache
	final_cache = new CacheMemory();
	final_cache->SetMaxBytesFromInfo(max_concurrent_frames * 4, info.width, info.height, info.sample_rate, info.channels);
}

// Delegating constructor that copies parameters from a provided ReaderInfo
Timeline::Timeline(const ReaderInfo info) : Timeline::Timeline(
	info.width, info.height, info.fps, info.sample_rate,
	info.channels, info.channel_layout) {}

// Constructor for the timeline (which loads a JSON structure from a file path, and initializes a timeline)
Timeline::Timeline(const std::string& projectPath, bool convert_absolute_paths) :
		is_open(false), auto_map_clips(true), managed_cache(true), path(projectPath),
		max_concurrent_frames(OPEN_MP_NUM_PROCESSORS), max_time(0.0) {

	// Create CrashHandler and Attach (incase of errors)
	CrashHandler::Instance();

	// Init final cache as NULL (will be created after loading json)
	final_cache = NULL;

	// Init viewport size (curve based, because it can be animated)
	viewport_scale = Keyframe(100.0);
	viewport_x = Keyframe(0.0);
	viewport_y = Keyframe(0.0);

	// Init background color
	color.red = Keyframe(0.0);
	color.green = Keyframe(0.0);
	color.blue = Keyframe(0.0);

	// Check if path exists
	QFileInfo filePath(QString::fromStdString(path));
	if (!filePath.exists()) {
		throw InvalidFile("File could not be opened.", path);
	}

	// Check OpenShot Install Path exists
	Settings *s = Settings::Instance();
	QDir openshotPath(QString::fromStdString(s->PATH_OPENSHOT_INSTALL));
	if (!openshotPath.exists()) {
		throw InvalidFile("PATH_OPENSHOT_INSTALL could not be found.", s->PATH_OPENSHOT_INSTALL);
	}
	QDir openshotTransPath(openshotPath.filePath("transitions"));
	if (!openshotTransPath.exists()) {
		throw InvalidFile("PATH_OPENSHOT_INSTALL/transitions could not be found.", openshotTransPath.path().toStdString());
	}

	// Determine asset path
	QString asset_name = filePath.baseName().left(30) + "_assets";
	QDir asset_folder(filePath.dir().filePath(asset_name));
	if (!asset_folder.exists()) {
		// Create directory if needed
		asset_folder.mkpath(".");
	}

	// Load UTF-8 project file into QString
	QFile projectFile(QString::fromStdString(path));
	projectFile.open(QFile::ReadOnly);
	QString projectContents = QString::fromUtf8(projectFile.readAll());

	// Convert all relative paths into absolute paths (if requested)
	if (convert_absolute_paths) {

		// Find all "image" or "path" references in JSON (using regex). Must loop through match results
		// due to our path matching needs, which are not possible with the QString::replace() function.
		QRegularExpression allPathsRegex(QStringLiteral("\"(image|path)\":.*?\"(.*?)\""));
		std::vector<QRegularExpressionMatch> matchedPositions;
		QRegularExpressionMatchIterator i = allPathsRegex.globalMatch(projectContents);
		while (i.hasNext()) {
			QRegularExpressionMatch match = i.next();
			if (match.hasMatch()) {
				// Push all match objects into a vector (so we can reverse them later)
				matchedPositions.push_back(match);
			}
		}

		// Reverse the matches (bottom of file to top, so our replacements don't break our match positions)
		std::vector<QRegularExpressionMatch>::reverse_iterator itr;
		for (itr = matchedPositions.rbegin(); itr != matchedPositions.rend(); itr++) {
			QRegularExpressionMatch match = *itr;
			QString relativeKey = match.captured(1); // image or path
			QString relativePath = match.captured(2); // relative file path
			QString absolutePath = "";

			// Find absolute path of all path, image (including special replacements of @assets and @transitions)
			if (relativePath.startsWith("@assets")) {
				absolutePath = QFileInfo(asset_folder.absoluteFilePath(relativePath.replace("@assets", "."))).canonicalFilePath();
			} else if (relativePath.startsWith("@transitions")) {
				absolutePath = QFileInfo(openshotTransPath.absoluteFilePath(relativePath.replace("@transitions", "."))).canonicalFilePath();
			} else {
				absolutePath = QFileInfo(filePath.absoluteDir().absoluteFilePath(relativePath)).canonicalFilePath();
			}

			// Replace path in JSON content, if an absolute path was successfully found
			if (!absolutePath.isEmpty()) {
				projectContents.replace(match.capturedStart(0), match.capturedLength(0), "\"" + relativeKey + "\": \"" + absolutePath + "\"");
			}
		}
		// Clear matches
		matchedPositions.clear();
	}

	// Set JSON of project
	SetJson(projectContents.toStdString());

	// Calculate valid duration and set has_audio and has_video
	// based on content inside this Timeline's clips.
	float calculated_duration = 0.0;
	for (auto clip : clips)
	{
		float clip_last_frame = clip->Position() + clip->Duration();
		if (clip_last_frame > calculated_duration)
			calculated_duration = clip_last_frame;
		if (clip->Reader() && clip->Reader()->info.has_audio)
			info.has_audio = true;
		if (clip->Reader() && clip->Reader()->info.has_video)
			info.has_video = true;

	}
	info.video_length = calculated_duration * info.fps.ToFloat();
	info.duration = calculated_duration;

	// Init FileInfo settings
	info.acodec = "openshot::timeline";
	info.vcodec = "openshot::timeline";
	info.video_timebase = info.fps.Reciprocal();
	info.has_video = true;
	info.has_audio = true;

	// Init max image size
	SetMaxSize(info.width, info.height);

	// Init cache
	final_cache = new CacheMemory();
	final_cache->SetMaxBytesFromInfo(max_concurrent_frames * 4, info.width, info.height, info.sample_rate, info.channels);
}

Timeline::~Timeline() {
	if (is_open) {
		// Auto Close if not already
		Close();
	}

	// Remove all clips, effects, and frame mappers
	Clear();

	// Destroy previous cache (if managed by timeline)
	if (managed_cache && final_cache) {
		delete final_cache;
		final_cache = NULL;
	}
}

// Add to the tracked_objects map a pointer to a tracked object (TrackedObjectBBox)
void Timeline::AddTrackedObject(std::shared_ptr<openshot::TrackedObjectBase> trackedObject){

	// Search for the tracked object on the map
	auto iterator = tracked_objects.find(trackedObject->Id());

	if (iterator != tracked_objects.end()){
		// Tracked object's id already present on the map, overwrite it
		iterator->second = trackedObject;
	}
	else{
		// Tracked object's id not present -> insert it on the map
		tracked_objects[trackedObject->Id()] = trackedObject;
	}

	return;
}

// Return tracked object pointer by it's id
std::shared_ptr<openshot::TrackedObjectBase> Timeline::GetTrackedObject(std::string id) const{

	// Search for the tracked object on the map
	auto iterator = tracked_objects.find(id);

	if (iterator != tracked_objects.end()){
		// Id found, return the pointer to the tracked object
		std::shared_ptr<openshot::TrackedObjectBase> trackedObject = iterator->second;
		return trackedObject;
	}
	else {
		// Id not found, return a null pointer
		return nullptr;
	}
}

// Return the ID's of the tracked objects as a list of strings
std::list<std::string> Timeline::GetTrackedObjectsIds() const{

	// Create a list of strings
	std::list<std::string> trackedObjects_ids;

	// Iterate through the tracked_objects map
	for (auto const& it: tracked_objects){
		// Add the IDs to the list
		trackedObjects_ids.push_back(it.first);
	}

	return trackedObjects_ids;
}

#ifdef USE_OPENCV
// Return the trackedObject's properties as a JSON string
std::string Timeline::GetTrackedObjectValues(std::string id, int64_t frame_number) const {

	// Initialize the JSON object
	Json::Value trackedObjectJson;

	// Search for the tracked object on the map
	auto iterator = tracked_objects.find(id);

	if (iterator != tracked_objects.end())
	{
		// Id found, Get the object pointer and cast it as a TrackedObjectBBox
		std::shared_ptr<TrackedObjectBBox> trackedObject = std::static_pointer_cast<TrackedObjectBBox>(iterator->second);

		// Get the trackedObject values for it's first frame
		if (trackedObject->ExactlyContains(frame_number)){
			BBox box = trackedObject->GetBox(frame_number);
			float x1 = box.cx - (box.width/2);
			float y1 = box.cy - (box.height/2);
			float x2 = box.cx + (box.width/2);
			float y2 = box.cy + (box.height/2);
			float rotation = box.angle;

			trackedObjectJson["x1"] = x1;
			trackedObjectJson["y1"] = y1;
			trackedObjectJson["x2"] = x2;
			trackedObjectJson["y2"] = y2;
			trackedObjectJson["rotation"] = rotation;

		} else {
			BBox box = trackedObject->BoxVec.begin()->second;
			float x1 = box.cx - (box.width/2);
			float y1 = box.cy - (box.height/2);
			float x2 = box.cx + (box.width/2);
			float y2 = box.cy + (box.height/2);
			float rotation = box.angle;

			trackedObjectJson["x1"] = x1;
			trackedObjectJson["y1"] = y1;
			trackedObjectJson["x2"] = x2;
			trackedObjectJson["y2"] = y2;
			trackedObjectJson["rotation"] = rotation;
		}

	}
	else {
		// Id not found, return all 0 values
		trackedObjectJson["x1"] = 0;
		trackedObjectJson["y1"] = 0;
		trackedObjectJson["x2"] = 0;
		trackedObjectJson["y2"] = 0;
		trackedObjectJson["rotation"] = 0;
	}

	return trackedObjectJson.toStyledString();
}
#endif

// Add an openshot::Clip to the timeline
void Timeline::AddClip(Clip* clip)
{
	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> guard(getFrameMutex);

	// Assign timeline to clip
	clip->ParentTimeline(this);

	// Clear cache of clip and nested reader (if any)
	if (clip->Reader() && clip->Reader()->GetCache())
		clip->Reader()->GetCache()->Clear();

	// All clips should be converted to the frame rate of this timeline
	if (auto_map_clips) {
		// Apply framemapper (or update existing framemapper)
		apply_mapper_to_clip(clip);
	}

	// Add clip to list
	clips.push_back(clip);

	// Sort clips
	sort_clips();
}

// Add an effect to the timeline
void Timeline::AddEffect(EffectBase* effect)
{
	// Assign timeline to effect
	effect->ParentTimeline(this);

	// Add effect to list
	effects.push_back(effect);

	// Sort effects
	sort_effects();
}

// Remove an effect from the timeline
void Timeline::RemoveEffect(EffectBase* effect)
{
	effects.remove(effect);

	// Delete effect object (if timeline allocated it)
	bool allocated = allocated_effects.count(effect);
	if (allocated) {
		delete effect;
		effect = NULL;
		allocated_effects.erase(effect);
	}

	// Sort effects
	sort_effects();
}

// Remove an openshot::Clip to the timeline
void Timeline::RemoveClip(Clip* clip)
{
	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> guard(getFrameMutex);

	clips.remove(clip);
	
	// Delete clip object (if timeline allocated it)
	bool allocated = allocated_clips.count(clip);
	if (allocated) {
		delete clip;
		clip = NULL;
		allocated_clips.erase(clip);
	}

	// Sort clips
	sort_clips();
}

// Look up a clip
openshot::Clip* Timeline::GetClip(const std::string& id)
{
	// Find the matching clip (if any)
	for (const auto& clip : clips) {
		if (clip->Id() == id) {
			return clip;
		}
	}
	return nullptr;
}

// Look up a timeline effect
openshot::EffectBase* Timeline::GetEffect(const std::string& id)
{
	// Find the matching effect (if any)
	for (const auto& effect : effects) {
		if (effect->Id() == id) {
			return effect;
		}
	}
	return nullptr;
}

openshot::EffectBase* Timeline::GetClipEffect(const std::string& id)
{
	// Search all clips for matching effect ID
	for (const auto& clip : clips) {
		const auto e = clip->GetEffect(id);
		if (e != nullptr) {
			return e;
		}
	}
	return nullptr;
}

// Return the list of effects on all clips
std::list<openshot::EffectBase*> Timeline::ClipEffects() const {

	// Initialize the list
	std::list<EffectBase*> timelineEffectsList;

	// Loop through all clips
	for (const auto& clip : clips) {

		// Get the clip's list of effects
		std::list<EffectBase*> clipEffectsList = clip->Effects();

		// Append the clip's effects to the list
		timelineEffectsList.insert(timelineEffectsList.end(), clipEffectsList.begin(), clipEffectsList.end());
	}

	return timelineEffectsList;
}

// Compute the end time of the latest timeline element
double Timeline::GetMaxTime() {
	// Return cached max_time variable (threadsafe)
	return max_time;
}

// Compute the highest frame# based on the latest time and FPS
int64_t Timeline::GetMaxFrame() {
	double fps = info.fps.ToDouble();
	auto max_time = GetMaxTime();
	return std::round(max_time * fps);
}

// Compute the start time of the first timeline clip
double Timeline::GetMinTime() {
	// Return cached min_time variable (threadsafe)
	return min_time;
}

// Compute the first frame# based on the first clip position
int64_t Timeline::GetMinFrame() {
	double fps = info.fps.ToDouble();
	auto min_time = GetMinTime();
	return std::round(min_time * fps) + 1;
}

// Apply a FrameMapper to a clip which matches the settings of this timeline
void Timeline::apply_mapper_to_clip(Clip* clip)
{
	// Determine type of reader
	ReaderBase* clip_reader = NULL;
	if (clip->Reader()->Name() == "FrameMapper")
	{
		// Get the existing reader
		clip_reader = (ReaderBase*) clip->Reader();

		// Update the mapping
		FrameMapper* clip_mapped_reader = (FrameMapper*) clip_reader;
		clip_mapped_reader->ChangeMapping(info.fps, PULLDOWN_NONE, info.sample_rate, info.channels, info.channel_layout);

	} else {

		// Create a new FrameMapper to wrap the current reader
		FrameMapper* mapper = new FrameMapper(clip->Reader(), info.fps, PULLDOWN_NONE, info.sample_rate, info.channels, info.channel_layout);
		allocated_frame_mappers.insert(mapper);
		clip_reader = (ReaderBase*) mapper;
	}

	// Update clip reader
	clip->Reader(clip_reader);
}

// Apply the timeline's framerate and samplerate to all clips
void Timeline::ApplyMapperToClips()
{
	// Clear all cached frames
	ClearAllCache();

	// Loop through all clips
	for (auto clip : clips)
	{
		// Apply framemapper (or update existing framemapper)
		apply_mapper_to_clip(clip);
	}
}

// Calculate time of a frame number, based on a framerate
double Timeline::calculate_time(int64_t number, Fraction rate)
{
	// Get float version of fps fraction
	double raw_fps = rate.ToFloat();

	// Return the time (in seconds) of this frame
	return double(number - 1) / raw_fps;
}

// Apply effects to the source frame (if any)
std::shared_ptr<Frame> Timeline::apply_effects(std::shared_ptr<Frame> frame, int64_t timeline_frame_number, int layer, TimelineInfoStruct* options)
{
	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod(
		"Timeline::apply_effects",
		"frame->number", frame->number,
		"timeline_frame_number", timeline_frame_number,
		"layer", layer);

	// Find Effects at this position and layer
	for (auto effect : effects)
	{
		// Does clip intersect the current requested time
		long effect_start_position = round(effect->Position() * info.fps.ToDouble()) + 1;
		long effect_end_position = round((effect->Position() + (effect->Duration())) * info.fps.ToDouble());

		bool does_effect_intersect = (effect_start_position <= timeline_frame_number && effect_end_position >= timeline_frame_number && effect->Layer() == layer);

		// Clip is visible
		if (does_effect_intersect)
		{
			// Determine the frame needed for this clip (based on the position on the timeline)
			long effect_start_frame = (effect->Start() * info.fps.ToDouble()) + 1;
			long effect_frame_number = timeline_frame_number - effect_start_position + effect_start_frame;

			if (!options->is_top_clip)
				continue; // skip effect, if overlapped/covered by another clip on same layer

			if (options->is_before_clip_keyframes != effect->info.apply_before_clip)
				continue; // skip effect, if this filter does not match

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod(
				"Timeline::apply_effects (Process Effect)",
				"effect_frame_number", effect_frame_number,
				"does_effect_intersect", does_effect_intersect);

			// Apply the effect to this frame
			frame = effect->GetFrame(frame, effect_frame_number);
		}

	} // end effect loop

	// Return modified frame
	return frame;
}

// Get or generate a blank frame
std::shared_ptr<Frame> Timeline::GetOrCreateFrame(std::shared_ptr<Frame> background_frame, Clip* clip, int64_t number, openshot::TimelineInfoStruct* options)
{
	std::shared_ptr<Frame> new_frame;

	// Init some basic properties about this frame
	int samples_in_frame = Frame::GetSamplesPerFrame(number, info.fps, info.sample_rate, info.channels);

	try {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod(
			"Timeline::GetOrCreateFrame (from reader)",
			"number", number,
			"samples_in_frame", samples_in_frame);

		// Attempt to get a frame (but this could fail if a reader has just been closed)
		new_frame = std::shared_ptr<Frame>(clip->GetFrame(background_frame, number, options));

		// Return real frame
		return new_frame;

	} catch (const ReaderClosed & e) {
		// ...
	} catch (const OutOfBoundsFrame & e) {
		// ...
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod(
		"Timeline::GetOrCreateFrame (create blank)",
		"number", number,
		"samples_in_frame", samples_in_frame);

	// Create blank frame
	return new_frame;
}

// Process a new layer of video or audio
void Timeline::add_layer(std::shared_ptr<Frame> new_frame, Clip* source_clip, int64_t clip_frame_number, bool is_top_clip, float max_volume)
{
	// Create timeline options (with details about this current frame request)
	TimelineInfoStruct* options = new TimelineInfoStruct();
	options->is_top_clip = is_top_clip;
	options->is_before_clip_keyframes = true;

	// Get the clip's frame, composited on top of the current timeline frame
	std::shared_ptr<Frame> source_frame;
	source_frame = GetOrCreateFrame(new_frame, source_clip, clip_frame_number, options);
	delete options;

	// No frame found... so bail
	if (!source_frame)
		return;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod(
		"Timeline::add_layer",
		"new_frame->number", new_frame->number,
		"clip_frame_number", clip_frame_number);

	/* COPY AUDIO - with correct volume */
	if (source_clip->Reader()->info.has_audio) {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod(
			"Timeline::add_layer (Copy Audio)",
			"source_clip->Reader()->info.has_audio", source_clip->Reader()->info.has_audio,
			"source_frame->GetAudioChannelsCount()", source_frame->GetAudioChannelsCount(),
			"info.channels", info.channels,
			"clip_frame_number", clip_frame_number);

		if (source_frame->GetAudioChannelsCount() == info.channels && source_clip->has_audio.GetInt(clip_frame_number) != 0)
			for (int channel = 0; channel < source_frame->GetAudioChannelsCount(); channel++)
			{
				// Get volume from previous frame and this frame
				float previous_volume = source_clip->volume.GetValue(clip_frame_number - 1);
				float volume = source_clip->volume.GetValue(clip_frame_number);
				int channel_filter = source_clip->channel_filter.GetInt(clip_frame_number); // optional channel to filter (if not -1)
				int channel_mapping = source_clip->channel_mapping.GetInt(clip_frame_number); // optional channel to map this channel to (if not -1)

				// Apply volume mixing strategy
				if (source_clip->mixing == VOLUME_MIX_AVERAGE && max_volume > 1.0) {
					// Don't allow this clip to exceed 100% (divide volume equally between all overlapping clips with volume
					previous_volume = previous_volume / max_volume;
					volume = volume / max_volume;
				}
				else if (source_clip->mixing == VOLUME_MIX_REDUCE && max_volume > 1.0) {
					// Reduce clip volume by a bit, hoping it will prevent exceeding 100% (but it is very possible it will)
					previous_volume = previous_volume * 0.77;
					volume = volume * 0.77;
				}

				// If channel filter enabled, check for correct channel (and skip non-matching channels)
				if (channel_filter != -1 && channel_filter != channel)
					continue; // skip to next channel

				// If no volume on this frame or previous frame, do nothing
				if (previous_volume == 0.0 && volume == 0.0)
					continue; // skip to next channel

				// If channel mapping disabled, just use the current channel
				if (channel_mapping == -1)
					channel_mapping = channel;

				// Apply ramp to source frame (if needed)
				if (!isEqual(previous_volume, 1.0) || !isEqual(volume, 1.0))
					source_frame->ApplyGainRamp(channel_mapping, 0, source_frame->GetAudioSamplesCount(), previous_volume, volume);

				// TODO: Improve FrameMapper (or Timeline) to always get the correct number of samples per frame.
				// Currently, the ResampleContext sometimes leaves behind a few samples for the next call, and the
				// number of samples returned is variable... and does not match the number expected.
				// This is a crude solution at best. =)
				if (new_frame->GetAudioSamplesCount() != source_frame->GetAudioSamplesCount()){
					// Force timeline frame to match the source frame
					new_frame->ResizeAudio(info.channels, source_frame->GetAudioSamplesCount(), info.sample_rate, info.channel_layout);
				}
				// Copy audio samples (and set initial volume).  Mix samples with existing audio samples.  The gains are added together, to
				// be sure to set the gain's correctly, so the sum does not exceed 1.0 (of audio distortion will happen).
				new_frame->AddAudio(false, channel_mapping, 0, source_frame->GetAudioSamples(channel), source_frame->GetAudioSamplesCount(), 1.0);
			}
		else
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod(
				"Timeline::add_layer (No Audio Copied - Wrong # of Channels)",
				"source_clip->Reader()->info.has_audio",
					source_clip->Reader()->info.has_audio,
				"source_frame->GetAudioChannelsCount()",
					source_frame->GetAudioChannelsCount(),
				"info.channels", info.channels,
				"clip_frame_number", clip_frame_number);
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod(
		"Timeline::add_layer (Transform: Composite Image Layer: Completed)",
		"source_frame->number", source_frame->number,
		"new_frame->GetImage()->width()", new_frame->GetWidth(),
		"new_frame->GetImage()->height()", new_frame->GetHeight());
}

// Update the list of 'opened' clips
void Timeline::update_open_clips(Clip *clip, bool does_clip_intersect)
{
	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> guard(getFrameMutex);

	ZmqLogger::Instance()->AppendDebugMethod(
		"Timeline::update_open_clips (before)",
		"does_clip_intersect", does_clip_intersect,
		"closing_clips.size()", closing_clips.size(),
		"open_clips.size()", open_clips.size());

	// is clip already in list?
	bool clip_found = open_clips.count(clip);

	if (clip_found && !does_clip_intersect)
	{
		// Remove clip from 'opened' list, because it's closed now
		open_clips.erase(clip);

		// Close clip
		clip->Close();
	}
	else if (!clip_found && does_clip_intersect)
	{
		// Add clip to 'opened' list, because it's missing
		open_clips[clip] = clip;

		try {
			// Open the clip
			clip->Open();

		} catch (const InvalidFile & e) {
			// ...
		}
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod(
		"Timeline::update_open_clips (after)",
		"does_clip_intersect", does_clip_intersect,
		"clip_found", clip_found,
		"closing_clips.size()", closing_clips.size(),
		"open_clips.size()", open_clips.size());
}

// Calculate the max and min duration (in seconds) of the timeline, based on all the clips, and cache the value
void Timeline::calculate_max_duration() {
	double last_clip = 0.0;
	double last_effect = 0.0;
	double first_clip = std::numeric_limits<double>::max();
	double first_effect = std::numeric_limits<double>::max();

	// Find the last and first clip
	if (!clips.empty()) {
		// Find the clip with the maximum end frame
		const auto max_clip = std::max_element(
			clips.begin(), clips.end(), CompareClipEndFrames());
		last_clip = (*max_clip)->Position() + (*max_clip)->Duration();

		// Find the clip with the minimum start position (ignoring layer)
		const auto min_clip = std::min_element(
			clips.begin(), clips.end(), [](const openshot::Clip* lhs, const openshot::Clip* rhs) {
				return lhs->Position() < rhs->Position();
			});
		first_clip = (*min_clip)->Position();
	}

	// Find the last and first effect
	if (!effects.empty()) {
		// Find the effect with the maximum end frame
		const auto max_effect = std::max_element(
			effects.begin(), effects.end(), CompareEffectEndFrames());
		last_effect = (*max_effect)->Position() + (*max_effect)->Duration();

		// Find the effect with the minimum start position
		const auto min_effect = std::min_element(
			effects.begin(), effects.end(), [](const openshot::EffectBase* lhs, const openshot::EffectBase* rhs) {
				return lhs->Position() < rhs->Position();
			});
		first_effect = (*min_effect)->Position();
	}

	// Calculate the max and min time
	max_time = std::max(last_clip, last_effect);
	min_time = std::min(first_clip, first_effect);

	// If no clips or effects exist, set min_time to 0
	if (clips.empty() && effects.empty()) {
		min_time = 0.0;
		max_time = 0.0;
	}
}

// Sort clips by position on the timeline
void Timeline::sort_clips()
{
	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> guard(getFrameMutex);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod(
		"Timeline::SortClips",
		"clips.size()", clips.size());

	// sort clips
	clips.sort(CompareClips());

	// calculate max timeline duration
	calculate_max_duration();
}

// Sort effects by position on the timeline
void Timeline::sort_effects()
{
	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> guard(getFrameMutex);

	// sort clips
	effects.sort(CompareEffects());

	// calculate max timeline duration
	calculate_max_duration();
}

// Clear all clips from timeline
void Timeline::Clear()
{
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::Clear");

	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> guard(getFrameMutex);

	// Close all open clips
	for (auto clip : clips)
	{
		update_open_clips(clip, false);

		// Delete clip object (if timeline allocated it)
		bool allocated = allocated_clips.count(clip);
		if (allocated) {
			delete clip;
		}
	}
	// Clear all clips
	clips.clear();
	allocated_clips.clear();

	// Close all effects
	for (auto effect : effects)
	{
		// Delete effect object (if timeline allocated it)
		bool allocated = allocated_effects.count(effect);
		if (allocated) {
			delete effect;
		}
	}
	// Clear all effects
	effects.clear();
	allocated_effects.clear();

	// Delete all FrameMappers
	for (auto mapper : allocated_frame_mappers)
	{
		mapper->Reader(NULL);
		mapper->Close();
		delete mapper;
	}
	allocated_frame_mappers.clear();
}

// Close the reader (and any resources it was consuming)
void Timeline::Close()
{
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::Close");

	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> guard(getFrameMutex);

	// Close all open clips
	for (auto clip : clips)
	{
		// Open or Close this clip, based on if it's intersecting or not
		update_open_clips(clip, false);
	}

	// Mark timeline as closed
	is_open = false;

	// Clear all cache (deep clear, including nested Readers)
	ClearAllCache(true);
}

// Open the reader (and start consuming resources)
void Timeline::Open()
{
	is_open = true;
}

// Compare 2 floating point numbers for equality
bool Timeline::isEqual(double a, double b)
{
	return fabs(a - b) < 0.000001;
}

// Get an openshot::Frame object for a specific frame number of this reader.
std::shared_ptr<Frame> Timeline::GetFrame(int64_t requested_frame)
{
	// Adjust out of bounds frame number
	if (requested_frame < 1)
		requested_frame = 1;

	// Check cache
	std::shared_ptr<Frame> frame;
	frame = final_cache->GetFrame(requested_frame);
	if (frame) {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod(
			"Timeline::GetFrame (Cached frame found)",
			"requested_frame", requested_frame);

		// Return cached frame
		return frame;
	}
	else
	{
		// Prevent async calls to the following code
		const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

		// Check cache 2nd time
		std::shared_ptr<Frame> frame;
		frame = final_cache->GetFrame(requested_frame);
		if (frame) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod(
					"Timeline::GetFrame (Cached frame found on 2nd check)",
					"requested_frame", requested_frame);

			// Return cached frame
			return frame;
		} else {
			// Get a list of clips that intersect with the requested section of timeline
			// This also opens the readers for intersecting clips, and marks non-intersecting clips as 'needs closing'
			std::vector<Clip *> nearby_clips;
			nearby_clips = find_intersecting_clips(requested_frame, 1, true);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod(
					"Timeline::GetFrame (processing frame)",
					"requested_frame", requested_frame,
					"omp_get_thread_num()", omp_get_thread_num());

			// Init some basic properties about this frame
			int samples_in_frame = Frame::GetSamplesPerFrame(requested_frame, info.fps, info.sample_rate, info.channels);

			// Create blank frame (which will become the requested frame)
			std::shared_ptr<Frame> new_frame(std::make_shared<Frame>(requested_frame, preview_width, preview_height, "#000000", samples_in_frame, info.channels));
			new_frame->AddAudioSilence(samples_in_frame);
			new_frame->SampleRate(info.sample_rate);
			new_frame->ChannelsLayout(info.channel_layout);

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod(
					"Timeline::GetFrame (Adding solid color)",
					"requested_frame", requested_frame,
					"info.width", info.width,
					"info.height", info.height);

			// Add Background Color to 1st layer (if animated or not black)
			if ((color.red.GetCount() > 1 || color.green.GetCount() > 1 || color.blue.GetCount() > 1) ||
				(color.red.GetValue(requested_frame) != 0.0 || color.green.GetValue(requested_frame) != 0.0 ||
				 color.blue.GetValue(requested_frame) != 0.0))
				new_frame->AddColor(preview_width, preview_height, color.GetColorHex(requested_frame));

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod(
					"Timeline::GetFrame (Loop through clips)",
					"requested_frame", requested_frame,
					"clips.size()", clips.size(),
					"nearby_clips.size()", nearby_clips.size());

			// Find Clips near this time
			for (auto clip : nearby_clips) {
				long clip_start_position = round(clip->Position() * info.fps.ToDouble()) + 1;
				long clip_end_position = round((clip->Position() + clip->Duration()) * info.fps.ToDouble());
				bool does_clip_intersect = (clip_start_position <= requested_frame && clip_end_position >= requested_frame);

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod(
						"Timeline::GetFrame (Does clip intersect)",
						"requested_frame", requested_frame,
						"clip->Position()", clip->Position(),
						"clip->Duration()", clip->Duration(),
						"does_clip_intersect", does_clip_intersect);

				// Clip is visible
				if (does_clip_intersect) {
					// Determine if clip is "top" clip on this layer (only happens when multiple clips are overlapping)
					bool is_top_clip = true;
					float max_volume = 0.0;
					for (auto nearby_clip : nearby_clips) {
						long nearby_clip_start_position = round(nearby_clip->Position() * info.fps.ToDouble()) + 1;
						long nearby_clip_end_position = round((nearby_clip->Position() + nearby_clip->Duration()) * info.fps.ToDouble()) + 1;
						long nearby_clip_start_frame = (nearby_clip->Start() * info.fps.ToDouble()) + 1;
						long nearby_clip_frame_number = requested_frame - nearby_clip_start_position + nearby_clip_start_frame;

						// Determine if top clip
						if (clip->Id() != nearby_clip->Id() && clip->Layer() == nearby_clip->Layer() &&
							nearby_clip_start_position <= requested_frame && nearby_clip_end_position >= requested_frame &&
							nearby_clip_start_position > clip_start_position && is_top_clip == true) {
							is_top_clip = false;
						}

						// Determine max volume of overlapping clips
						if (nearby_clip->Reader() && nearby_clip->Reader()->info.has_audio &&
							nearby_clip->has_audio.GetInt(nearby_clip_frame_number) != 0 &&
							nearby_clip_start_position <= requested_frame && nearby_clip_end_position >= requested_frame) {
							max_volume += nearby_clip->volume.GetValue(nearby_clip_frame_number);
						}
					}

					// Determine the frame needed for this clip (based on the position on the timeline)
					long clip_start_frame = (clip->Start() * info.fps.ToDouble()) + 1;
					long clip_frame_number = requested_frame - clip_start_position + clip_start_frame;

					// Debug output
					ZmqLogger::Instance()->AppendDebugMethod(
							"Timeline::GetFrame (Calculate clip's frame #)",
							"clip->Position()", clip->Position(),
							"clip->Start()", clip->Start(),
							"info.fps.ToFloat()", info.fps.ToFloat(),
							"clip_frame_number", clip_frame_number);

					// Add clip's frame as layer
					add_layer(new_frame, clip, clip_frame_number, is_top_clip, max_volume);

				} else {
					// Debug output
					ZmqLogger::Instance()->AppendDebugMethod(
							"Timeline::GetFrame (clip does not intersect)",
							"requested_frame", requested_frame,
							"does_clip_intersect", does_clip_intersect);
				}

			} // end clip loop

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod(
					"Timeline::GetFrame (Add frame to cache)",
					"requested_frame", requested_frame,
					"info.width", info.width,
					"info.height", info.height);

			// Set frame # on mapped frame
			new_frame->SetFrameNumber(requested_frame);

			// Add final frame to cache
			final_cache->Add(new_frame);

			// Return frame (or blank frame)
			return new_frame;
		}
	}
}


// Find intersecting clips (or non intersecting clips)
std::vector<Clip*> Timeline::find_intersecting_clips(int64_t requested_frame, int number_of_frames, bool include)
{
	// Find matching clips
	std::vector<Clip*> matching_clips;

	// Calculate time of frame
	float min_requested_frame = requested_frame;
	float max_requested_frame = requested_frame + (number_of_frames - 1);

	// Find Clips at this time
	for (auto clip : clips)
	{
		// Does clip intersect the current requested time
		long clip_start_position = round(clip->Position() * info.fps.ToDouble()) + 1;
		long clip_end_position = round((clip->Position() + clip->Duration()) * info.fps.ToDouble()) + 1;

		bool does_clip_intersect =
				(clip_start_position <= min_requested_frame || clip_start_position <= max_requested_frame) &&
				(clip_end_position >= min_requested_frame || clip_end_position >= max_requested_frame);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod(
			"Timeline::find_intersecting_clips (Is clip near or intersecting)",
			"requested_frame", requested_frame,
			"min_requested_frame", min_requested_frame,
			"max_requested_frame", max_requested_frame,
			"clip->Position()", clip->Position(),
			"does_clip_intersect", does_clip_intersect);

		// Open (or schedule for closing) this clip, based on if it's intersecting or not
		update_open_clips(clip, does_clip_intersect);

		// Clip is visible
		if (does_clip_intersect && include)
			// Add the intersecting clip
			matching_clips.push_back(clip);

		else if (!does_clip_intersect && !include)
			// Add the non-intersecting clip
			matching_clips.push_back(clip);

	} // end clip loop

	// return list
	return matching_clips;
}

// Set the cache object used by this reader
void Timeline::SetCache(CacheBase* new_cache) {
	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

	// Destroy previous cache (if managed by timeline)
	if (managed_cache && final_cache) {
		delete final_cache;
		final_cache = NULL;
		managed_cache = false;
	}

	// Set new cache
	final_cache = new_cache;
}

// Generate JSON string of this object
std::string Timeline::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Timeline::JsonValue() const {

	// Create root json object
	Json::Value root = ReaderBase::JsonValue(); // get parent properties
	root["type"] = "Timeline";
	root["viewport_scale"] = viewport_scale.JsonValue();
	root["viewport_x"] = viewport_x.JsonValue();
	root["viewport_y"] = viewport_y.JsonValue();
	root["color"] = color.JsonValue();
	root["path"] = path;

	// Add array of clips
	root["clips"] = Json::Value(Json::arrayValue);

	// Find Clips at this time
	for (const auto existing_clip : clips)
	{
		root["clips"].append(existing_clip->JsonValue());
	}

	// Add array of effects
	root["effects"] = Json::Value(Json::arrayValue);

	// loop through effects
	for (const auto existing_effect: effects)
	{
		root["effects"].append(existing_effect->JsonValue());
	}

	// return JsonValue
	return root;
}

// Load JSON string into this object
void Timeline::SetJson(const std::string value) {

	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

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
void Timeline::SetJsonValue(const Json::Value root) {

	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

	// Close timeline before we do anything (this closes all clips)
	bool was_open = is_open;
	Close();

	// Set parent data
	ReaderBase::SetJsonValue(root);

	// Set data from Json (if key is found)
	if (!root["path"].isNull())
		path = root["path"].asString();

	if (!root["clips"].isNull()) {
		// Clear existing clips
		clips.clear();

		// loop through clips
		for (const Json::Value existing_clip : root["clips"]) {
			// Skip NULL nodes
			if (existing_clip.isNull()) {
				continue;
			}

			// Create Clip
			Clip *c = new Clip();

			// Keep track of allocated clip objects
			allocated_clips.insert(c);

			// When a clip is attached to an object, it searches for the object
			// on it's parent timeline. Setting the parent timeline of the clip here
			// allows attaching it to an object when exporting the project (because)
			// the exporter script initializes the clip and it's effects
			// before setting its parent timeline.
			c->ParentTimeline(this);

			// Load Json into Clip
			c->SetJsonValue(existing_clip);

			// Add Clip to Timeline
			AddClip(c);
		}
	}

	if (!root["effects"].isNull()) {
		// Clear existing effects
		effects.clear();

		// loop through effects
		for (const Json::Value existing_effect :root["effects"]) {
			// Skip NULL nodes
			if (existing_effect.isNull()) {
				continue;
			}

			// Create Effect
			EffectBase *e = NULL;

			if (!existing_effect["type"].isNull()) {
				// Create instance of effect
				if ( (e = EffectInfo().CreateEffect(existing_effect["type"].asString())) ) {

					// Keep track of allocated effect objects
					allocated_effects.insert(e);

					// Load Json into Effect
					e->SetJsonValue(existing_effect);

					// Add Effect to Timeline
					AddEffect(e);
				}
			}
		}
	}

	if (!root["duration"].isNull()) {
		// Update duration of timeline
		info.duration = root["duration"].asDouble();
		info.video_length = info.fps.ToFloat() * info.duration;
	}

	// Update preview settings
	preview_width = info.width;
	preview_height = info.height;

	// Resort (and recalculate min/max duration)
	sort_clips();
	sort_effects();

	// Re-open if needed
	if (was_open)
		Open();
}

// Apply a special formatted JSON object, which represents a change to the timeline (insert, update, delete)
void Timeline::ApplyJsonDiff(std::string value) {

	// Get lock (prevent getting frames while this happens)
	const std::lock_guard<std::recursive_mutex> lock(getFrameMutex);

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Process the JSON change array, loop through each item
		for (const Json::Value change : root) {
			std::string change_key = change["key"][(uint)0].asString();

			// Process each type of change
			if (change_key == "clips")
				// Apply to CLIPS
				apply_json_to_clips(change);

			else if (change_key == "effects")
				// Apply to EFFECTS
				apply_json_to_effects(change);

			else
				// Apply to TIMELINE
				apply_json_to_timeline(change);

		}
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Apply JSON diff to clips
void Timeline::apply_json_to_clips(Json::Value change) {

	// Get key and type of change
	std::string change_type = change["type"].asString();
	std::string clip_id = "";
	Clip *existing_clip = NULL;

	// Find id of clip (if any)
	for (auto key_part : change["key"]) {
		// Get each change
		if (key_part.isObject()) {
			// Check for id
			if (!key_part["id"].isNull()) {
				// Set the id
				clip_id = key_part["id"].asString();

				// Find matching clip in timeline (if any)
				for (auto c : clips)
				{
					if (c->Id() == clip_id) {
						existing_clip = c;
						break; // clip found, exit loop
					}
				}
				break; // id found, exit loop
			}
		}
	}

	// Check for a more specific key (targetting this clip's effects)
	// For example: ["clips", {"id:123}, "effects", {"id":432}]
	if (existing_clip && change["key"].size() == 4 && change["key"][2] == "effects")
	{
		// This change is actually targetting a specific effect under a clip (and not the clip)
		Json::Value key_part = change["key"][3];

		if (key_part.isObject()) {
			// Check for id
			if (!key_part["id"].isNull())
			{
				// Set the id
				std::string effect_id = key_part["id"].asString();

				// Find matching effect in timeline (if any)
				std::list<EffectBase*> effect_list = existing_clip->Effects();
				for (auto e : effect_list)
				{
					if (e->Id() == effect_id) {
						// Apply the change to the effect directly
						apply_json_to_effects(change, e);

						// Calculate start and end frames that this impacts, and remove those frames from the cache
						int64_t new_starting_frame = (existing_clip->Position() * info.fps.ToDouble()) + 1;
						int64_t new_ending_frame = ((existing_clip->Position() + existing_clip->Duration()) * info.fps.ToDouble()) + 1;
						final_cache->Remove(new_starting_frame - 8, new_ending_frame + 8);

						return; // effect found, don't update clip
					}
				}
			}
		}
	}

	// Determine type of change operation
	if (change_type == "insert") {

		// Create clip
		Clip *clip = new Clip();

		// Keep track of allocated clip objects
		allocated_clips.insert(clip);

		// Set properties of clip from JSON
		clip->SetJsonValue(change["value"]);

		// Add clip to timeline
		AddClip(clip);

	} else if (change_type == "update") {

		// Update existing clip
		if (existing_clip) {
			// Update clip properties from JSON
			existing_clip->SetJsonValue(change["value"]);

			// Calculate start and end frames that this impacts, and remove those frames from the cache
			int64_t old_starting_frame = (existing_clip->Position() * info.fps.ToDouble()) + 1;
			int64_t old_ending_frame = ((existing_clip->Position() + existing_clip->Duration()) * info.fps.ToDouble()) + 1;
			final_cache->Remove(old_starting_frame - 8, old_ending_frame + 8);

			// Remove cache on clip's Reader (if found)
			if (existing_clip->Reader() && existing_clip->Reader()->GetCache())
				existing_clip->Reader()->GetCache()->Remove(old_starting_frame - 8, old_ending_frame + 8);

			// Apply framemapper (or update existing framemapper)
			if (auto_map_clips) {
				apply_mapper_to_clip(existing_clip);
			}
		}

	} else if (change_type == "delete") {

		// Remove existing clip
		if (existing_clip) {
			// Remove clip from timeline
			RemoveClip(existing_clip);

			// Calculate start and end frames that this impacts, and remove those frames from the cache
			int64_t old_starting_frame = (existing_clip->Position() * info.fps.ToDouble()) + 1;
			int64_t old_ending_frame = ((existing_clip->Position() + existing_clip->Duration()) * info.fps.ToDouble()) + 1;
			final_cache->Remove(old_starting_frame - 8, old_ending_frame + 8);
		}

	}

	// Calculate start and end frames that this impacts, and remove those frames from the cache
	if (!change["value"].isArray() && !change["value"]["position"].isNull()) {
		int64_t new_starting_frame = (change["value"]["position"].asDouble() * info.fps.ToDouble()) + 1;
		int64_t new_ending_frame = ((change["value"]["position"].asDouble() + change["value"]["end"].asDouble() - change["value"]["start"].asDouble()) * info.fps.ToDouble()) + 1;
		final_cache->Remove(new_starting_frame - 8, new_ending_frame + 8);
	}

	// Re-Sort Clips (since they likely changed)
	sort_clips();
}

// Apply JSON diff to effects
void Timeline::apply_json_to_effects(Json::Value change) {

	// Get key and type of change
	std::string change_type = change["type"].asString();
	EffectBase *existing_effect = NULL;

	// Find id of an effect (if any)
	for (auto key_part : change["key"]) {

		if (key_part.isObject()) {
			// Check for id
			if (!key_part["id"].isNull())
			{
				// Set the id
				std::string effect_id = key_part["id"].asString();

				// Find matching effect in timeline (if any)
				for (auto e : effects)
				{
					if (e->Id() == effect_id) {
						existing_effect = e;
						break; // effect found, exit loop
					}
				}
				break; // id found, exit loop
			}
		}
	}

	// Now that we found the effect, apply the change to it
	if (existing_effect || change_type == "insert") {
		// Apply change to effect
		apply_json_to_effects(change, existing_effect);
	}
}

// Apply JSON diff to effects (if you already know which effect needs to be updated)
void Timeline::apply_json_to_effects(Json::Value change, EffectBase* existing_effect) {

	// Get key and type of change
	std::string change_type = change["type"].asString();

	// Calculate start and end frames that this impacts, and remove those frames from the cache
	if (!change["value"].isArray() && !change["value"]["position"].isNull()) {
		int64_t new_starting_frame = (change["value"]["position"].asDouble() * info.fps.ToDouble()) + 1;
		int64_t new_ending_frame = ((change["value"]["position"].asDouble() + change["value"]["end"].asDouble() - change["value"]["start"].asDouble()) * info.fps.ToDouble()) + 1;
		final_cache->Remove(new_starting_frame - 8, new_ending_frame + 8);
	}

	// Determine type of change operation
	if (change_type == "insert") {

		// Determine type of effect
		std::string effect_type = change["value"]["type"].asString();

		// Create Effect
		EffectBase *e = NULL;

		// Init the matching effect object
		if ( (e = EffectInfo().CreateEffect(effect_type)) ) {

			// Keep track of allocated effect objects
			allocated_effects.insert(e);

			// Load Json into Effect
			e->SetJsonValue(change["value"]);

			// Add Effect to Timeline
			AddEffect(e);
		}

	} else if (change_type == "update") {

		// Update existing effect
		if (existing_effect) {

			// Calculate start and end frames that this impacts, and remove those frames from the cache
			int64_t old_starting_frame = (existing_effect->Position() * info.fps.ToDouble()) + 1;
			int64_t old_ending_frame = ((existing_effect->Position() + existing_effect->Duration()) * info.fps.ToDouble()) + 1;
			final_cache->Remove(old_starting_frame - 8, old_ending_frame + 8);

			// Update effect properties from JSON
			existing_effect->SetJsonValue(change["value"]);
		}

	} else if (change_type == "delete") {

		// Remove existing effect
		if (existing_effect) {

			// Calculate start and end frames that this impacts, and remove those frames from the cache
			int64_t old_starting_frame = (existing_effect->Position() * info.fps.ToDouble()) + 1;
			int64_t old_ending_frame = ((existing_effect->Position() + existing_effect->Duration()) * info.fps.ToDouble()) + 1;
			final_cache->Remove(old_starting_frame - 8, old_ending_frame + 8);

			// Remove effect from timeline
			RemoveEffect(existing_effect);
		}

	}

	// Re-Sort Effects (since they likely changed)
	sort_effects();
}

// Apply JSON diff to timeline properties
void Timeline::apply_json_to_timeline(Json::Value change) {
	bool cache_dirty = true;

	// Get key and type of change
	std::string change_type = change["type"].asString();
	std::string root_key = change["key"][(uint)0].asString();
	std::string sub_key = "";
	if (change["key"].size() >= 2)
		sub_key = change["key"][(uint)1].asString();

	// Determine type of change operation
	if (change_type == "insert" || change_type == "update") {

		// INSERT / UPDATE
		// Check for valid property
		if (root_key == "color")
			// Set color
			color.SetJsonValue(change["value"]);
		else if (root_key == "viewport_scale")
			// Set viewport scale
			viewport_scale.SetJsonValue(change["value"]);
		else if (root_key == "viewport_x")
			// Set viewport x offset
			viewport_x.SetJsonValue(change["value"]);
		else if (root_key == "viewport_y")
			// Set viewport y offset
			viewport_y.SetJsonValue(change["value"]);
		else if (root_key == "duration") {
			// Update duration of timeline
			info.duration = change["value"].asDouble();
			info.video_length = info.fps.ToFloat() * info.duration;

			// We don't want to clear cache for duration adjustments
			cache_dirty = false;
		}
		else if (root_key == "width") {
			// Set width
			info.width = change["value"].asInt();
			preview_width = info.width;
		}
		else if (root_key == "height") {
			// Set height
			info.height = change["value"].asInt();
			preview_height = info.height;
		}
		else if (root_key == "fps" && sub_key == "" && change["value"].isObject()) {
			// Set fps fraction
			if (!change["value"]["num"].isNull())
				info.fps.num = change["value"]["num"].asInt();
			if (!change["value"]["den"].isNull())
				info.fps.den = change["value"]["den"].asInt();
		}
		else if (root_key == "fps" && sub_key == "num")
			// Set fps.num
			info.fps.num = change["value"].asInt();
		else if (root_key == "fps" && sub_key == "den")
			// Set fps.den
			info.fps.den = change["value"].asInt();
		else if (root_key == "display_ratio" && sub_key == "" && change["value"].isObject()) {
			// Set display_ratio fraction
			if (!change["value"]["num"].isNull())
				info.display_ratio.num = change["value"]["num"].asInt();
			if (!change["value"]["den"].isNull())
				info.display_ratio.den = change["value"]["den"].asInt();
		}
		else if (root_key == "display_ratio" && sub_key == "num")
			// Set display_ratio.num
			info.display_ratio.num = change["value"].asInt();
		else if (root_key == "display_ratio" && sub_key == "den")
			// Set display_ratio.den
			info.display_ratio.den = change["value"].asInt();
		else if (root_key == "pixel_ratio" && sub_key == "" && change["value"].isObject()) {
			// Set pixel_ratio fraction
			if (!change["value"]["num"].isNull())
				info.pixel_ratio.num = change["value"]["num"].asInt();
			if (!change["value"]["den"].isNull())
				info.pixel_ratio.den = change["value"]["den"].asInt();
		}
		else if (root_key == "pixel_ratio" && sub_key == "num")
			// Set pixel_ratio.num
			info.pixel_ratio.num = change["value"].asInt();
		else if (root_key == "pixel_ratio" && sub_key == "den")
			// Set pixel_ratio.den
			info.pixel_ratio.den = change["value"].asInt();

		else if (root_key == "sample_rate")
			// Set sample rate
			info.sample_rate = change["value"].asInt();
		else if (root_key == "channels")
			// Set channels
			info.channels = change["value"].asInt();
		else if (root_key == "channel_layout")
			// Set channel layout
			info.channel_layout = (ChannelLayout) change["value"].asInt();
		else
			// Error parsing JSON (or missing keys)
			throw InvalidJSONKey("JSON change key is invalid", change.toStyledString());


	} else if (change["type"].asString() == "delete") {

		// DELETE / RESET
		// Reset the following properties (since we can't delete them)
		if (root_key == "color") {
			color = Color();
			color.red = Keyframe(0.0);
			color.green = Keyframe(0.0);
			color.blue = Keyframe(0.0);
		}
		else if (root_key == "viewport_scale")
			viewport_scale = Keyframe(1.0);
		else if (root_key == "viewport_x")
			viewport_x = Keyframe(0.0);
		else if (root_key == "viewport_y")
			viewport_y = Keyframe(0.0);
		else
			// Error parsing JSON (or missing keys)
			throw InvalidJSONKey("JSON change key is invalid", change.toStyledString());

	}

	if (cache_dirty) {
		// Clear entire cache
		ClearAllCache();
	}
}

// Clear all caches
void Timeline::ClearAllCache(bool deep) {

	// Clear primary cache
	if (final_cache) {
		final_cache->Clear();
	}

	// Loop through all clips
	try {
		for (const auto clip : clips) {
			// Clear cache on clip
			clip->Reader()->GetCache()->Clear();

			// Clear nested Reader (if deep clear requested)
			if (deep && clip->Reader()->Name() == "FrameMapper") {
				FrameMapper *nested_reader = static_cast<FrameMapper *>(clip->Reader());
				if (nested_reader->Reader() && nested_reader->Reader()->GetCache())
					nested_reader->Reader()->GetCache()->Clear();
			}

			// Clear clip cache
			clip->GetCache()->Clear();
		}
	} catch (const ReaderClosed & e) {
		// ...
	}
}

// Set Max Image Size (used for performance optimization). Convenience function for setting
// Settings::Instance()->MAX_WIDTH and Settings::Instance()->MAX_HEIGHT.
void Timeline::SetMaxSize(int width, int height) {
	// Maintain aspect ratio regardless of what size is passed in
	QSize display_ratio_size = QSize(info.width, info.height);
	QSize proposed_size = QSize(std::min(width, info.width), std::min(height, info.height));

	// Scale QSize up to proposed size
	display_ratio_size.scale(proposed_size, Qt::KeepAspectRatio);

	// Update preview settings
	preview_width = display_ratio_size.width();
	preview_height = display_ratio_size.height();
}
