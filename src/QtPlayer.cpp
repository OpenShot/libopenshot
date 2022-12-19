/**
 * @file
 * @brief Source file for QtPlayer class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "QtPlayer.h"

#include "AudioDevices.h"
#include "Clip.h"
#include "FFmpegReader.h"
#include "Timeline.h"
#include "Qt/PlayerPrivate.h"
#include "Qt/VideoRenderer.h"

namespace openshot
{

    using AudioDeviceList = std::vector<std::pair<std::string, std::string>>;

    // Delegating constructor
    QtPlayer::QtPlayer()
    : QtPlayer::QtPlayer(new VideoRenderer())
    { }

    // Constructor
    QtPlayer::QtPlayer(openshot::RendererBase *rb)
    : PlayerBase()
    , p(new openshot::PlayerPrivate(rb))
    , threads_started(false)
    {
        reader = NULL;
    }

    QtPlayer::~QtPlayer()
    {
        if (mode != PLAYBACK_STOPPED)
            Stop();

        delete p;
    }

    void QtPlayer::CloseAudioDevice()
    {
    	// Close audio device (only do this once, when all audio playback is finished)
    	openshot::AudioDeviceManagerSingleton::Instance()->CloseAudioDevice();
    }

    // Return any error string during initialization
    std::string QtPlayer::GetError() {
    	if (reader && threads_started) {
    		return p->audioPlayback->getError();
    	} else {
    		return "";
    	}
    }

    // Return the default audio sample rate (from the system)
    double QtPlayer::GetDefaultSampleRate() {
        if (reader && threads_started) {
            return p->audioPlayback->getDefaultSampleRate();
        } else {
            return 0;
        }
    }

    /// Get vector of audio device names & types
    AudioDeviceList QtPlayer::GetAudioDeviceNames() {
        AudioDevices devs;
        return devs.getNames();
    }

    // Get current audio device or last attempted (if none succeeded)
    AudioDeviceInfo QtPlayer::GetCurrentAudioDevice() {
        return p->audioPlayback->getCurrentAudioDevice();
    }

    // Set the source JSON of an openshot::Timelime
    void QtPlayer::SetTimelineSource(const std::string &json) {
        // Create timeline instance (720p, since we have no re-scaling in this player yet)
        reader = new Timeline(1280, 720, openshot::Fraction(30, 1), 44100, 2, openshot::LAYOUT_STEREO);

        Timeline* tm = (Timeline*)reader;
        tm->SetJson(json);
        tm->DisplayInfo();
        tm->Open();

        // Set the reader
        Reader(reader);
    }

    void QtPlayer::SetSource(const std::string &source)
    {
    	FFmpegReader *ffreader = new FFmpegReader(source);
    	ffreader->DisplayInfo();

    	// Use default sample rate (or use the FFmpegReader's audio settings if any)
    	int sample_rate = 44100;
    	if (ffreader->info.sample_rate > 0)
    	    sample_rate = ffreader->info.sample_rate;

        // Use default channels (or use the FFmpegReader's audio settings if any)
    	int channels = 2;
    	if (ffreader->info.channels > 0)
    	    channels = ffreader->info.channels;

        // Use default channel layout (or use the FFmpegReader's audio settings if any)
    	openshot::ChannelLayout channel_layout = openshot::LAYOUT_STEREO;
    	if (channels != 2)
            channel_layout = ffreader->info.channel_layout;

    	// Create timeline instance (720p, since we have no re-scaling in this player yet)
    	reader = new Timeline(1280, 720, ffreader->info.fps, sample_rate, channels, channel_layout);
    	Clip *c = new Clip(source);

    	Timeline* tm = (Timeline*)reader;
    	tm->AddClip(c);
    	tm->Open();

        // Set the reader
        Reader(reader);
    }

    void QtPlayer::Play()
    {
        // Set mode to playing, and speed to normal
    	mode = PLAYBACK_PLAY;
    	Speed(1);

    	if (reader && !threads_started) {
    		// Start thread only once
    		p->startPlayback();
    		threads_started = true;
    	}
    }

    void QtPlayer::Loading()
    {
        mode = PLAYBACK_LOADING;
    }

    /// Get the current mode
    openshot::PlaybackMode QtPlayer::Mode()
    {
    	return mode;
    }

    void QtPlayer::Pause()
    {
        mode = PLAYBACK_PAUSED;
        Speed(0);
    }

    int64_t QtPlayer::Position()
    {
        return p->video_position;
    }

    void QtPlayer::Seek(int64_t new_frame)
    {
    	// Check for seek
    	if (reader && threads_started && new_frame > 0) {
    		// Notify cache thread that seek has occurred
    		p->videoCache->Seek(new_frame, true);

            // Notify audio thread that seek has occurred
            p->audioPlayback->Seek(new_frame);

    		// Update current position
    		p->Seek(new_frame);
    	}
    }

    void QtPlayer::Stop()
    {
    	// Change mode to stopped
    	mode = PLAYBACK_STOPPED;

    	// Notify threads of stopping
    	if (reader && threads_started) {
    		p->videoCache->Stop();
    		p->audioPlayback->Stop();

    		// Kill all threads
    		p->stopPlayback();
    	}

    	p->video_position = 0;
    	threads_started = false;
    }

    // Set the reader object
    void QtPlayer::Reader(openshot::ReaderBase *new_reader)
    {
    	// Set new reader. Note: Be sure to close and dispose of the old reader after calling this
    	reader = new_reader;
    	p->reader = new_reader;
    	p->videoCache->Reader(new_reader);
    	p->audioPlayback->Reader(new_reader);
    }

    // Get the current reader, such as a FFmpegReader
    openshot::ReaderBase* QtPlayer::Reader() {
    	return reader;
    }

    // Set the QWidget pointer to display the video on (as a LONG pointer id)
    void QtPlayer::SetQWidget(int64_t qwidget_address) {
    	// Update override QWidget address on the video renderer
    	p->renderer->OverrideWidget(qwidget_address);
    }

    // Get the Renderer pointer address (for Python to cast back into a QObject)
    int64_t QtPlayer::GetRendererQObject() {
    	return (int64_t)(VideoRenderer*)p->renderer;
    }

    // Get the Playback speed
    float QtPlayer::Speed() {
    	return speed;
    }

    // Set the Playback speed multiplier (1.0 = normal speed, <1.0 = slower, >1.0 faster)
    void QtPlayer::Speed(float new_speed) {
    	speed = new_speed;
    	p->speed = new_speed;
    	p->videoCache->setSpeed(new_speed);
    	if (p->reader && p->reader->info.has_audio) {
            p->audioPlayback->setSpeed(new_speed);
    	}
    }

    // Get the Volume
    float QtPlayer::Volume() {
    	return volume;
    }

    // Set the Volume multiplier (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
    void QtPlayer::Volume(float new_volume) {
    	volume = new_volume;
    }
}
