/**
 * @file
 * @brief Source file for QtPlayer class
 * @author Duzy Chan <code@duzy.info>
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#include "../include/Clip.h"
#include "../include/FFmpegReader.h"
#include "../include/Timeline.h"
#include "../include/QtPlayer.h"
#include "../include/Qt/PlayerPrivate.h"
#include "../include/Qt/VideoRenderer.h"

using namespace openshot;

QtPlayer::QtPlayer() : PlayerBase(), p(new PlayerPrivate(new VideoRenderer())), threads_started(false)
{
	reader = NULL;
}

QtPlayer::QtPlayer(RendererBase *rb) : PlayerBase(), p(new PlayerPrivate(rb)), threads_started(false)
{
	reader = NULL;
}

QtPlayer::~QtPlayer()
{
    if (mode != PLAYBACK_STOPPED) {
    	Stop();
    }
    delete p;
}

void QtPlayer::SetSource(const std::string &source)
{
    reader = new FFmpegReader(source);
    reader->debug = false;
    reader->Open();

    // experimental timeline code
	//Clip *c = new Clip(source);
	//c->scale = SCALE_NONE;
	//c->rotation.AddPoint(1, 0.0);
	//c->rotation.AddPoint(1000, 360.0);
	//c->Waveform(true);

    //Timeline *t = new Timeline(c->Reader()->info.width, c->Reader()->info.height, c->Reader()->info.fps, c->Reader()->info.sample_rate, c->Reader()->info.channels);
	//Timeline *t = new Timeline(1280, 720, openshot::Fraction(24,1), 44100, 2, LAYOUT_STEREO);
    //t->debug = true; openshot::Fraction(30,1)
    //t->info = c->Reader()->info;
    //t->info.fps = openshot::Fraction(12,1);
    //t->GetCache()->SetMaxBytesFromInfo(40, c->Reader()->info.width, c->Reader()->info.height, c->Reader()->info.sample_rate, c->Reader()->info.channels);

    //t->AddClip(c);
    //t->Open();

    // Set the reader
	Reader(reader);
}

void QtPlayer::Play()
{
	cout << "PLAY() on QTPlayer" << endl;

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
PlaybackMode QtPlayer::Mode()
{
	return mode;
}

void QtPlayer::Pause()
{
    mode = PLAYBACK_PAUSED;
    Speed(0);
}

int QtPlayer::Position()
{
    return p->video_position;
}

void QtPlayer::Seek(int new_frame)
{
	// Check for seek
	if (new_frame > 0) {
		// Notify cache thread that seek has occurred
		p->videoCache->Seek(new_frame);

		// Update current position
		p->video_position = new_frame;

		// Clear last position (to force refresh)
		p->last_video_position = 0;

		// Notify audio thread that seek has occurred
		p->audioPlayback->Seek(new_frame);
	}
}

void QtPlayer::Stop()
{
	// Change mode to stopped
	mode = PLAYBACK_STOPPED;

	// Notify threads of stopping
	p->videoCache->Stop();
	p->audioPlayback->Stop();

	// Kill all threads
	p->stopPlayback();

	p->video_position = 0;
	threads_started = false;
}

// Set the reader object
void QtPlayer::Reader(ReaderBase *new_reader)
{
	cout << "Reader SET: " << new_reader << endl;
	reader = new_reader;
	p->reader = new_reader;
	p->videoCache->Reader(new_reader);
	p->audioPlayback->Reader(new_reader);
}

// Get the current reader, such as a FFmpegReader
ReaderBase* QtPlayer::Reader() {
	return reader;
}

// Set the QWidget pointer to display the video on (as a LONG pointer id)
void QtPlayer::SetQWidget(long qwidget_address) {
	// Update override QWidget address on the video renderer
	p->renderer->OverrideWidget(qwidget_address);
}

// Get the Renderer pointer address (for Python to cast back into a QObject)
long QtPlayer::GetRendererQObject() {
	return (long) (VideoRenderer*)p->renderer;
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
	if (p->reader->info.has_audio)
		p->audioPlayback->setSpeed(new_speed);
}

// Get the Volume
float QtPlayer::Volume() {
	return volume;
}

// Set the Volume multiplier (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
void QtPlayer::Volume(float new_volume) {
	volume = new_volume;
}
