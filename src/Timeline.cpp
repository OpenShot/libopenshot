/**
 * @file
 * @brief Source file for Timeline class
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

#include "Timeline.h"

using namespace openshot;

// Default Constructor for the timeline (which sets the canvas width and height)
Timeline::Timeline(int width, int height, Fraction fps, int sample_rate, int channels, ChannelLayout channel_layout) :
		is_open(false), auto_map_clips(true), managed_cache(true), path("")
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

	// Configure OpenMP parallelism
	// Default number of threads per block
	omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
	// Allow nested parallel sections as deeply as supported
	omp_set_max_active_levels(OPEN_MP_MAX_ACTIVE);

	// Init max image size
	SetMaxSize(info.width, info.height);

	// Init cache
	final_cache = new CacheMemory();
	final_cache->SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
}

// Constructor for the timeline (which loads a JSON structure from a file path, and initializes a timeline)
Timeline::Timeline(const std::string& projectPath, bool convert_absolute_paths) :
		is_open(false), auto_map_clips(true), managed_cache(true), path(projectPath) {

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

	// Configure OpenMP parallelism
	// Default number of threads per section
	omp_set_num_threads(OPEN_MP_NUM_PROCESSORS);
	// Allow nested parallel sections as deeply as supported
	omp_set_max_active_levels(OPEN_MP_MAX_ACTIVE);

	// Init max image size
	SetMaxSize(info.width, info.height);

	// Init cache
	final_cache = new CacheMemory();
	final_cache->SetMaxBytesFromInfo(OPEN_MP_NUM_PROCESSORS * 2, info.width, info.height, info.sample_rate, info.channels);
}

Timeline::~Timeline() {
	if (is_open)
		// Auto Close if not already
		Close();

	// Free all allocated frame mappers
	std::set<FrameMapper *>::iterator it;
	for (it = allocated_frame_mappers.begin(); it != allocated_frame_mappers.end(); ) {
		// Dereference and clean up FrameMapper object
		FrameMapper *mapper = (*it);
		mapper->Reader(NULL);
		mapper->Close();
		delete mapper;
		// Remove reference and proceed to next element
		it = allocated_frame_mappers.erase(it);
	}

	// Destroy previous cache (if managed by timeline)
	if (managed_cache && final_cache) {
		delete final_cache;
		final_cache = NULL;
	}
}

// Add an openshot::Clip to the timeline
void Timeline::AddClip(Clip* clip)
{
	// Assign timeline to clip
	clip->ParentTimeline(this);

	// Clear cache of clip and nested reader (if any)
	clip->cache.Clear();
	if (clip->Reader() && clip->Reader()->GetCache())
		clip->Reader()->GetCache()->Clear();

	// All clips should be converted to the frame rate of this timeline
	if (auto_map_clips)
		// Apply framemapper (or update existing framemapper)
		apply_mapper_to_clip(clip);

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
}

// Remove an openshot::Clip to the timeline
void Timeline::RemoveClip(Clip* clip)
{
	clips.remove(clip);
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

// Compute the end time of the latest timeline element
double Timeline::GetMaxTime() {
	double last_clip = 0.0;
	double last_effect = 0.0;

	if (!clips.empty()) {
		const auto max_clip = std::max_element(
				clips.begin(), clips.end(), CompareClipEndFrames());
		last_clip = (*max_clip)->Position() + (*max_clip)->Duration();
	}
	if (!effects.empty()) {
		const auto max_effect = std::max_element(
				effects.begin(), effects.end(), CompareEffectEndFrames());
		last_effect = (*max_effect)->Position() + (*max_effect)->Duration();
	}
	return std::max(last_clip, last_effect);
}

// Compute the highest frame# based on the latest time and FPS
int64_t Timeline::GetMaxFrame() {
	double fps = info.fps.ToDouble();
	auto max_time = GetMaxTime();
	return std::round(max_time * fps) + 1;
}

// Apply a FrameMapper to a clip which matches the settings of this timeline
void Timeline::apply_mapper_to_clip(Clip* clip)
{
    // Get lock (prevent getting frames while this happens)
    const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

	// Determine type of reader
	ReaderBase* clip_reader = NULL;
	if (clip->Reader()->Name() == "FrameMapper")
	{
		// Get the existing reader
		clip_reader = (ReaderBase*) clip->Reader();

	} else {

		// Create a new FrameMapper to wrap the current reader
		FrameMapper* mapper = new FrameMapper(clip->Reader(), info.fps, PULLDOWN_NONE, info.sample_rate, info.channels, info.channel_layout);
		allocated_frame_mappers.insert(mapper);
		clip_reader = (ReaderBase*) mapper;
	}

	// Update the mapping
	FrameMapper* clip_mapped_reader = (FrameMapper*) clip_reader;
	clip_mapped_reader->ChangeMapping(info.fps, PULLDOWN_NONE, info.sample_rate, info.channels, info.channel_layout);

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
std::shared_ptr<Frame> Timeline::apply_effects(std::shared_ptr<Frame> frame, int64_t timeline_frame_number, int layer)
{
	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::apply_effects", "frame->number", frame->number, "timeline_frame_number", timeline_frame_number, "layer", layer);

	// Find Effects at this position and layer
	for (auto effect : effects)
	{
		// Does clip intersect the current requested time
		long effect_start_position = round(effect->Position() * info.fps.ToDouble()) + 1;
		long effect_end_position = round((effect->Position() + (effect->Duration())) * info.fps.ToDouble()) + 1;

		bool does_effect_intersect = (effect_start_position <= timeline_frame_number && effect_end_position >= timeline_frame_number && effect->Layer() == layer);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::apply_effects (Does effect intersect)", "effect->Position()", effect->Position(), "does_effect_intersect", does_effect_intersect, "timeline_frame_number", timeline_frame_number, "layer", layer);

		// Clip is visible
		if (does_effect_intersect)
		{
			// Determine the frame needed for this clip (based on the position on the timeline)
            long effect_start_frame = (effect->Start() * info.fps.ToDouble()) + 1;
			long effect_frame_number = timeline_frame_number - effect_start_position + effect_start_frame;

			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Timeline::apply_effects (Process Effect)", "effect_frame_number", effect_frame_number, "does_effect_intersect", does_effect_intersect);

			// Apply the effect to this frame
			frame = effect->GetFrame(frame, effect_frame_number);
		}

	} // end effect loop

	// Return modified frame
	return frame;
}

// Get or generate a blank frame
std::shared_ptr<Frame> Timeline::GetOrCreateFrame(Clip* clip, int64_t number)
{
	std::shared_ptr<Frame> new_frame;

	// Init some basic properties about this frame
	int samples_in_frame = Frame::GetSamplesPerFrame(number, info.fps, info.sample_rate, info.channels);

	try {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetOrCreateFrame (from reader)", "number", number, "samples_in_frame", samples_in_frame);

		// Attempt to get a frame (but this could fail if a reader has just been closed)
		#pragma omp critical (T_GetOtCreateFrame)
		new_frame = std::shared_ptr<Frame>(clip->GetFrame(number));

		// Return real frame
		return new_frame;

	} catch (const ReaderClosed & e) {
		// ...
	} catch (const TooManySeeks & e) {
		// ...
	} catch (const OutOfBoundsFrame & e) {
		// ...
	}

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetOrCreateFrame (create blank)", "number", number, "samples_in_frame", samples_in_frame);

	// Create blank frame
	new_frame = std::make_shared<Frame>(number, preview_width, preview_height, "#000000", samples_in_frame, info.channels);
	#pragma omp critical (T_GetOtCreateFrame)
	{
		new_frame->SampleRate(info.sample_rate);
		new_frame->ChannelsLayout(info.channel_layout);
	}
	return new_frame;
}

// Process a new layer of video or audio
void Timeline::add_layer(std::shared_ptr<Frame> new_frame, Clip* source_clip, int64_t clip_frame_number, int64_t timeline_frame_number, bool is_top_clip, float max_volume)
{
	// Get the clip's frame & image
	std::shared_ptr<Frame> source_frame;
	#pragma omp critical (T_addLayer)
	source_frame = GetOrCreateFrame(source_clip, clip_frame_number);

	// No frame found... so bail
	if (!source_frame)
		return;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer", "new_frame->number", new_frame->number, "clip_frame_number", clip_frame_number, "timeline_frame_number", timeline_frame_number);

	/* Apply effects to the source frame (if any). If multiple clips are overlapping, only process the
	 * effects on the top clip. */
	if (is_top_clip) {
		#pragma omp critical (T_addLayer)
		source_frame = apply_effects(source_frame, timeline_frame_number, source_clip->Layer());
	}

	// Declare an image to hold the source frame's image
	std::shared_ptr<QImage> source_image;

	/* COPY AUDIO - with correct volume */
	if (source_clip->Reader()->info.has_audio) {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Copy Audio)", "source_clip->Reader()->info.has_audio", source_clip->Reader()->info.has_audio, "source_frame->GetAudioChannelsCount()", source_frame->GetAudioChannelsCount(), "info.channels", info.channels, "clip_frame_number", clip_frame_number, "timeline_frame_number", timeline_frame_number);

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
					#pragma omp critical (T_addLayer)

					new_frame->ResizeAudio(info.channels, source_frame->GetAudioSamplesCount(), info.sample_rate, info.channel_layout);
				}
				// Copy audio samples (and set initial volume).  Mix samples with existing audio samples.  The gains are added together, to
				// be sure to set the gain's correctly, so the sum does not exceed 1.0 (of audio distortion will happen).
				#pragma omp critical (T_addLayer)
				new_frame->AddAudio(false, channel_mapping, 0, source_frame->GetAudioSamples(channel), source_frame->GetAudioSamplesCount(), 1.0);

			}
		else
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (No Audio Copied - Wrong # of Channels)", "source_clip->Reader()->info.has_audio", source_clip->Reader()->info.has_audio, "source_frame->GetAudioChannelsCount()", source_frame->GetAudioChannelsCount(), "info.channels", info.channels, "clip_frame_number", clip_frame_number, "timeline_frame_number", timeline_frame_number);
	}

	// Skip out if video was disabled or only an audio frame (no visualisation in use)
	if (source_clip->has_video.GetInt(clip_frame_number) == 0 ||
	    (!source_clip->Waveform() && !source_clip->Reader()->info.has_video))
		// Skip the rest of the image processing for performance reasons
		return;

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Get Source Image)", "source_frame->number", source_frame->number, "source_clip->Waveform()", source_clip->Waveform(), "clip_frame_number", clip_frame_number);

	// Get actual frame image data
	source_image = source_frame->GetImage();

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Transform: Composite Image Layer: Prepare)", "source_frame->number", source_frame->number, "new_frame->GetImage()->width()", new_frame->GetImage()->width(), "source_image->width()", source_image->width());

	/* COMPOSITE SOURCE IMAGE (LAYER) ONTO FINAL IMAGE */
	std::shared_ptr<QImage> new_image;
	new_image = new_frame->GetImage();

	// Load timeline's new frame image into a QPainter
	QPainter painter(new_image.get());

	// Composite a new layer onto the image
	painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.drawImage(0, 0, *source_image, 0, 0, source_image->width(), source_image->height());
	painter.end();

	// Add new QImage to frame
	new_frame->AddImage(new_image);

	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::add_layer (Transform: Composite Image Layer: Completed)", "source_frame->number", source_frame->number, "new_frame->GetImage()->width()", new_frame->GetImage()->width());
}

// Update the list of 'opened' clips
void Timeline::update_open_clips(Clip *clip, bool does_clip_intersect)
{
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::update_open_clips (before)", "does_clip_intersect", does_clip_intersect, "closing_clips.size()", closing_clips.size(), "open_clips.size()", open_clips.size());

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
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::update_open_clips (after)", "does_clip_intersect", does_clip_intersect, "clip_found", clip_found, "closing_clips.size()", closing_clips.size(), "open_clips.size()", open_clips.size());
}

// Sort clips by position on the timeline
void Timeline::sort_clips()
{
	// Debug output
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::SortClips", "clips.size()", clips.size());

	// sort clips
	clips.sort(CompareClips());
}

// Sort effects by position on the timeline
void Timeline::sort_effects()
{
	// sort clips
	effects.sort(CompareEffects());
}

// Close the reader (and any resources it was consuming)
void Timeline::Close()
{
	ZmqLogger::Instance()->AppendDebugMethod("Timeline::Close");

	// Close all open clips
	for (auto clip : clips)
	{
		// Open or Close this clip, based on if it's intersecting or not
		update_open_clips(clip, false);
	}

	// Mark timeline as closed
	is_open = false;

	// Clear cache
	if (final_cache)
		final_cache->Clear();
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
	std::lock_guard<std::mutex> guard(get_frame_mutex);
	#pragma omp critical (T_GetFrame)
	frame = final_cache->GetFrame(requested_frame);
	if (frame) {
		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Cached frame found)", "requested_frame", requested_frame);

		// Return cached frame
		return frame;
	}
	else
	{
		// Create a scoped lock, allowing only a single thread to run the following code at one time
		const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

		// Check for open reader (or throw exception)
		if (!is_open)
			throw ReaderClosed("The Timeline is closed.  Call Open() before calling this method.");

		// Check cache again (due to locking)
		#pragma omp critical (T_GetFrame)
		frame = final_cache->GetFrame(requested_frame);
		if (frame) {
			// Debug output
			ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Cached frame found on 2nd look)", "requested_frame", requested_frame);

			// Return cached frame
			return frame;
		}

		// Check if previous frame was cached? (if not, assume we are seeking somewhere else on the Timeline, and need
		// to clear all cache (for continuity sake). For example, jumping back to a previous spot can cause issues with audio
		// data where the new jump location doesn't match up with the previously cached audio data.
		std::shared_ptr<Frame> previous_frame = final_cache->GetFrame(requested_frame - 1);
		if (!previous_frame) {
			// Seeking to new place on timeline (destroy cache)
			ClearAllCache();
		}

		// Minimum number of frames to process (for performance reasons)
		int minimum_frames = OPEN_MP_NUM_PROCESSORS;

		// Get a list of clips that intersect with the requested section of timeline
		// This also opens the readers for intersecting clips, and marks non-intersecting clips as 'needs closing'
		std::vector<Clip*> nearby_clips;
		#pragma omp critical (T_GetFrame)
		nearby_clips = find_intersecting_clips(requested_frame, minimum_frames, true);

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame", "requested_frame", requested_frame, "minimum_frames", minimum_frames, "OPEN_MP_NUM_PROCESSORS", OPEN_MP_NUM_PROCESSORS);

		// GENERATE CACHE FOR CLIPS (IN FRAME # SEQUENCE)
		// Determine all clip frames, and request them in order (to keep resampled audio in sequence)
		for (int64_t frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
		{
			// Loop through clips
			for (auto clip : nearby_clips)
			{
                long clip_start_position = round(clip->Position() * info.fps.ToDouble()) + 1;
                long clip_end_position = round((clip->Position() + clip->Duration()) * info.fps.ToDouble()) + 1;

				bool does_clip_intersect = (clip_start_position <= frame_number && clip_end_position >= frame_number);
				if (does_clip_intersect)
				{
					// Get clip frame #
                    long clip_start_frame = (clip->Start() * info.fps.ToDouble()) + 1;
					long clip_frame_number = frame_number - clip_start_position + clip_start_frame;

					// Cache clip object
					clip->GetFrame(clip_frame_number);
				}
			}
		}

		#pragma omp parallel
		{
			// Loop through all requested frames
			#pragma omp for ordered firstprivate(nearby_clips, requested_frame, minimum_frames) schedule(static,1)
			for (int64_t frame_number = requested_frame; frame_number < requested_frame + minimum_frames; frame_number++)
			{
				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (processing frame)", "frame_number", frame_number, "omp_get_thread_num()", omp_get_thread_num());

				// Init some basic properties about this frame
				int samples_in_frame = Frame::GetSamplesPerFrame(frame_number, info.fps, info.sample_rate, info.channels);

				// Create blank frame (which will become the requested frame)
				std::shared_ptr<Frame> new_frame(std::make_shared<Frame>(frame_number, preview_width, preview_height, "#000000", samples_in_frame, info.channels));
				#pragma omp critical (T_GetFrame)
				{
					new_frame->AddAudioSilence(samples_in_frame);
					new_frame->SampleRate(info.sample_rate);
					new_frame->ChannelsLayout(info.channel_layout);
				}

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Adding solid color)", "frame_number", frame_number, "info.width", info.width, "info.height", info.height);

				// Add Background Color to 1st layer (if animated or not black)
				if ((color.red.GetCount() > 1 || color.green.GetCount() > 1 || color.blue.GetCount() > 1) ||
					(color.red.GetValue(frame_number) != 0.0 || color.green.GetValue(frame_number) != 0.0 || color.blue.GetValue(frame_number) != 0.0))
				new_frame->AddColor(preview_width, preview_height, color.GetColorHex(frame_number));

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Loop through clips)", "frame_number", frame_number, "clips.size()", clips.size(), "nearby_clips.size()", nearby_clips.size());

				// Find Clips near this time
				for (auto clip : nearby_clips)
				{
                    long clip_start_position = round(clip->Position() * info.fps.ToDouble()) + 1;
                    long clip_end_position = round((clip->Position() + clip->Duration()) * info.fps.ToDouble()) + 1;

                    bool does_clip_intersect = (clip_start_position <= frame_number && clip_end_position >= frame_number);

					// Debug output
					ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Does clip intersect)", "frame_number", frame_number, "clip->Position()", clip->Position(), "clip->Duration()", clip->Duration(), "does_clip_intersect", does_clip_intersect);

					// Clip is visible
					if (does_clip_intersect)
					{
						// Determine if clip is "top" clip on this layer (only happens when multiple clips are overlapping)
						bool is_top_clip = true;
						float max_volume = 0.0;
						for (auto nearby_clip : nearby_clips)
						{
                            long nearby_clip_start_position = round(nearby_clip->Position() * info.fps.ToDouble()) + 1;
                            long nearby_clip_end_position = round((nearby_clip->Position() + nearby_clip->Duration()) * info.fps.ToDouble()) + 1;
							long nearby_clip_start_frame = (nearby_clip->Start() * info.fps.ToDouble()) + 1;
							long nearby_clip_frame_number = frame_number - nearby_clip_start_position + nearby_clip_start_frame;

							// Determine if top clip
							if (clip->Id() != nearby_clip->Id() && clip->Layer() == nearby_clip->Layer() &&
                                    nearby_clip_start_position <= frame_number && nearby_clip_end_position >= frame_number &&
                                    nearby_clip_start_position > clip_start_position && is_top_clip == true) {
								is_top_clip = false;
							}

							// Determine max volume of overlapping clips
							if (nearby_clip->Reader() && nearby_clip->Reader()->info.has_audio &&
									nearby_clip->has_audio.GetInt(nearby_clip_frame_number) != 0 &&
									nearby_clip_start_position <= frame_number && nearby_clip_end_position >= frame_number) {
									max_volume += nearby_clip->volume.GetValue(nearby_clip_frame_number);
							}
						}

						// Determine the frame needed for this clip (based on the position on the timeline)
                        long clip_start_frame = (clip->Start() * info.fps.ToDouble()) + 1;
						long clip_frame_number = frame_number - clip_start_position + clip_start_frame;

						// Debug output
						ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Calculate clip's frame #)", "clip->Position()", clip->Position(), "clip->Start()", clip->Start(), "info.fps.ToFloat()", info.fps.ToFloat(), "clip_frame_number", clip_frame_number);

						// Add clip's frame as layer
						add_layer(new_frame, clip, clip_frame_number, frame_number, is_top_clip, max_volume);

					} else
						// Debug output
						ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (clip does not intersect)", "frame_number", frame_number, "does_clip_intersect", does_clip_intersect);

				} // end clip loop

				// Debug output
				ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (Add frame to cache)", "frame_number", frame_number, "info.width", info.width, "info.height", info.height);

				// Set frame # on mapped frame
				#pragma omp ordered
				{
					new_frame->SetFrameNumber(frame_number);

					// Add final frame to cache
					final_cache->Add(new_frame);
				}

			} // end frame loop
		} // end parallel

		// Debug output
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::GetFrame (end parallel region)", "requested_frame", requested_frame, "omp_get_thread_num()", omp_get_thread_num());

		// Return frame (or blank frame)
		return final_cache->GetFrame(requested_frame);
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

	// Re-Sort Clips (since they likely changed)
	sort_clips();

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
		ZmqLogger::Instance()->AppendDebugMethod("Timeline::find_intersecting_clips (Is clip near or intersecting)", "requested_frame", requested_frame, "min_requested_frame", min_requested_frame, "max_requested_frame", max_requested_frame, "clip->Position()", clip->Position(), "does_clip_intersect", does_clip_intersect);

		// Open (or schedule for closing) this clip, based on if it's intersecting or not
		#pragma omp critical (reader_lock)
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
	const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

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

	// Close timeline before we do anything (this also removes all open and closing clips)
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
			// Create Clip
			Clip *c = new Clip();

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
			// Create Effect
			EffectBase *e = NULL;

			if (!existing_effect["type"].isNull()) {
				// Create instance of effect
				if ( (e = EffectInfo().CreateEffect(existing_effect["type"].asString())) ) {

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

	// Re-open if needed
	if (was_open)
		Open();
}

// Apply a special formatted JSON object, which represents a change to the timeline (insert, update, delete)
void Timeline::ApplyJsonDiff(std::string value) {

    // Get lock (prevent getting frames while this happens)
    const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

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

	// Calculate start and end frames that this impacts, and remove those frames from the cache
	if (!change["value"].isArray() && !change["value"]["position"].isNull()) {
		int64_t new_starting_frame = (change["value"]["position"].asDouble() * info.fps.ToDouble()) + 1;
		int64_t new_ending_frame = ((change["value"]["position"].asDouble() + change["value"]["end"].asDouble() - change["value"]["start"].asDouble()) * info.fps.ToDouble()) + 1;
		final_cache->Remove(new_starting_frame - 8, new_ending_frame + 8);
	}

	// Determine type of change operation
	if (change_type == "insert") {

		// Create new clip
		Clip *clip = new Clip();
		clip->SetJsonValue(change["value"]); // Set properties of new clip from JSON
		AddClip(clip); // Add clip to timeline

		// Apply framemapper (or update existing framemapper)
		apply_mapper_to_clip(clip);

	} else if (change_type == "update") {

		// Update existing clip
		if (existing_clip) {

			// Calculate start and end frames that this impacts, and remove those frames from the cache
			int64_t old_starting_frame = (existing_clip->Position() * info.fps.ToDouble()) + 1;
			int64_t old_ending_frame = ((existing_clip->Position() + existing_clip->Duration()) * info.fps.ToDouble()) + 1;
			final_cache->Remove(old_starting_frame - 8, old_ending_frame + 8);

            // Remove cache on clip's Reader (if found)
            if (existing_clip->Reader() && existing_clip->Reader()->GetCache())
                existing_clip->Reader()->GetCache()->Remove(old_starting_frame - 8, old_ending_frame + 8);

			// Update clip properties from JSON
			existing_clip->SetJsonValue(change["value"]);

			// Apply framemapper (or update existing framemapper)
			apply_mapper_to_clip(existing_clip);
		}

	} else if (change_type == "delete") {

		// Remove existing clip
		if (existing_clip) {

			// Calculate start and end frames that this impacts, and remove those frames from the cache
			int64_t old_starting_frame = (existing_clip->Position() * info.fps.ToDouble()) + 1;
			int64_t old_ending_frame = ((existing_clip->Position() + existing_clip->Duration()) * info.fps.ToDouble()) + 1;
			final_cache->Remove(old_starting_frame - 8, old_ending_frame + 8);

			// Remove clip from timeline
			RemoveClip(existing_clip);
		}

	}

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
	if (existing_effect || change_type == "insert")
		// Apply change to effect
		apply_json_to_effects(change, existing_effect);
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

			// Load Json into Effect
			e->SetJsonValue(change["value"]);

			// Add Effect to Timeline
			AddEffect(e);

			// Clear cache on parent clip (if any)
			Clip* parent_clip = (Clip*) e->ParentClip();
			if (parent_clip && parent_clip->GetCache()) {
				parent_clip->GetCache()->Clear();
			}
		}

	} else if (change_type == "update") {

		// Update existing effect
		if (existing_effect) {

			// Calculate start and end frames that this impacts, and remove those frames from the cache
			int64_t old_starting_frame = (existing_effect->Position() * info.fps.ToDouble()) + 1;
			int64_t old_ending_frame = ((existing_effect->Position() + existing_effect->Duration()) * info.fps.ToDouble()) + 1;
			final_cache->Remove(old_starting_frame - 8, old_ending_frame + 8);

			// Clear cache on parent clip (if any)
			Clip* parent_clip = (Clip*) existing_effect->ParentClip();
			if (parent_clip && parent_clip->GetCache()) {
				parent_clip->GetCache()->Clear();
			}

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

			// Clear cache on parent clip (if any)
			Clip* parent_clip = (Clip*) existing_effect->ParentClip();
			if (parent_clip && parent_clip->GetCache()) {
				parent_clip->GetCache()->Clear();
			}

			// Remove effect from timeline
			RemoveEffect(existing_effect);
		}

	}
}

// Apply JSON diff to timeline properties
void Timeline::apply_json_to_timeline(Json::Value change) {

	// Get key and type of change
	std::string change_type = change["type"].asString();
	std::string root_key = change["key"][(uint)0].asString();
	std::string sub_key = "";
	if (change["key"].size() >= 2)
		sub_key = change["key"][(uint)1].asString();

	// Clear entire cache
	ClearAllCache();

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

}

// Clear all caches
void Timeline::ClearAllCache() {

	// Get lock (prevent getting frames while this happens)
	const GenericScopedLock<CriticalSection> lock(getFrameCriticalSection);

    // Clear primary cache
    final_cache->Clear();

    // Loop through all clips
    for (auto clip : clips)
    {
        // Clear cache on clip
		clip->GetCache()->Clear();
        clip->Reader()->GetCache()->Clear();

        // Clear nested Reader (if any)
        if (clip->Reader()->Name() == "FrameMapper") {
			FrameMapper* nested_reader = (FrameMapper*) clip->Reader();
			if (nested_reader->Reader() && nested_reader->Reader()->GetCache())
				nested_reader->Reader()->GetCache()->Clear();
		}

    }
}

// Set Max Image Size (used for performance optimization). Convenience function for setting
// Settings::Instance()->MAX_WIDTH and Settings::Instance()->MAX_HEIGHT.
void Timeline::SetMaxSize(int width, int height) {
	// Maintain aspect ratio regardless of what size is passed in
	QSize display_ratio_size = QSize(info.display_ratio.num * info.pixel_ratio.ToFloat(), info.display_ratio.den * info.pixel_ratio.ToFloat());
	QSize proposed_size = QSize(std::min(width, info.width), std::min(height, info.height));

	// Scale QSize up to proposed size
	display_ratio_size.scale(proposed_size, Qt::KeepAspectRatio);

	// Update preview settings
	preview_width = display_ratio_size.width();
	preview_height = display_ratio_size.height();
}
