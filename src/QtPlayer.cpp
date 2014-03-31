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
 * and/or modify it under the terms of the GNU Affero General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Also, if your software can interact with users remotely through a computer
 * network, you should also make sure that it provides a way for users to
 * get its source. For example, if your program is a web application, its
 * interface could display a "Source" link that leads users to an archive
 * of the code. There are many ways you could offer source, and different
 * solutions will be better for different programs; see section 13 for the
 * specific requirements.
 *
 * You should also get your employer (if you work as a programmer) or school,
 * if any, to sign a "copyright disclaimer" for the program, if necessary.
 * For more information on this, and how to apply and follow the GNU AGPL, see
 * <http://www.gnu.org/licenses/>.
 */

#include "../include/FFmpegReader.h"
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
    reader->Open();
	p->Reader(reader);
}

void QtPlayer::Play()
{
	cout << "PLAY() on QTPlayer" << endl;
	if (reader && !threads_started) {
		mode = PLAYBACK_PLAY;
		p->Reader(reader);
		p->startPlayback();
		threads_started = true;
	} else {
		mode = PLAYBACK_PLAY;
		Speed(1);
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
	// Seek the reader to a new position
    p->Seek(new_frame);
}

void QtPlayer::Stop()
{
    mode = PLAYBACK_STOPPED;
	p->stopPlayback();
	p->video_position = 0;
	threads_started = false;
}

// Set the reader object
void QtPlayer::Reader(ReaderBase *new_reader)
{
	reader = new_reader;
	p->Reader(new_reader);
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

// Get the Playback speed
float QtPlayer::Speed() {
	return speed;
}

// Set the Playback speed multiplier (1.0 = normal speed, <1.0 = slower, >1.0 faster)
void QtPlayer::Speed(float new_speed) {
	speed = new_speed;
	p->Speed(new_speed);
}

// Get the Volume
float QtPlayer::Volume() {
	return volume;
}

// Set the Volume multiplier (1.0 = normal volume, <1.0 = quieter, >1.0 louder)
void QtPlayer::Volume(float new_volume) {
	volume = new_volume;
}
