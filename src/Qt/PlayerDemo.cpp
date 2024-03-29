/**
 * @file
 * @brief Source file for Demo QtPlayer application
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <string>

#include "PlayerDemo.h"
#include "../QtPlayer.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QWidget>
#include <QBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QApplication>

PlayerDemo::PlayerDemo(QWidget *parent)
    : QWidget(parent)
    , vbox(new QVBoxLayout(this))
    , menu(new QMenuBar(this))
    , video(new VideoRenderWidget(this))
    , player(new openshot::QtPlayer(video->GetRenderer()))
{
    setWindowTitle("OpenShot Player");

    menu->setNativeMenuBar(false);

    QAction *action = NULL;
    action = menu->addAction("Choose File");
    connect(action, SIGNAL(triggered(bool)), this, SLOT(open(bool)));

    vbox->addWidget(menu, 0);
    vbox->addWidget(video, 1);

    vbox->setMargin(0);
    vbox->setSpacing(0);
    resize(600, 480);

    // Accept keyboard event
    setFocusPolicy(Qt::StrongFocus);

}

PlayerDemo::~PlayerDemo()
{
}

void PlayerDemo::closeEvent(QCloseEvent *event)
{
	// Close window, stop player, and quit
	QWidget *pWin = QApplication::activeWindow();
	pWin->hide();
	player->Stop();
	QApplication::quit();
}

void PlayerDemo::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Space || event->key() == Qt::Key_K) {

		if (player->Mode() == openshot::PLAYBACK_PAUSED)
		{
			// paused, so start playing again
			player->Play();

		}
		else if (player->Mode() == openshot::PLAYBACK_PLAY)
		{

			if (player->Speed() == 0)
				// already playing, but speed is zero... so just speed up to normal
				player->Speed(1);
			else
				// already playing... so pause
				player->Pause();

		}

	}
	else if (event->key() == Qt::Key_J) {
		if (player->Speed() - 1 != 0)
			player->Speed(player->Speed() - 1);
		else
			player->Speed(player->Speed() - 2);

		if (player->Mode() == openshot::PLAYBACK_PAUSED)
			player->Play();
	}
	else if (event->key() == Qt::Key_L) {
		if (player->Speed() + 1 != 0)
			player->Speed(player->Speed() + 1);
		else
			player->Speed(player->Speed() + 2);

		if (player->Mode() == openshot::PLAYBACK_PAUSED)
			player->Play();

	}
	else if (event->key() == Qt::Key_Left) {
		if (player->Speed() != 0)
			player->Speed(0);
		player->Seek(player->Position() - 1);
	}
	else if (event->key() == Qt::Key_Right) {
		if (player->Speed() != 0)
			player->Speed(0);
		player->Seek(player->Position() + 1);
	}
	else if (event->key() == Qt::Key_Escape) {
		QWidget *pWin = QApplication::activeWindow();
		pWin->hide();

		player->Stop();

		QApplication::quit();
	}

	event->accept();
	QWidget::keyPressEvent(event);
}

void PlayerDemo::open(bool checked)
{
	// Get filename of media files
    const QString filename = QFileDialog::getOpenFileName(this, "Open Video File");
    if (filename.isEmpty()) return;

    // Open *.osp file (read JSON into variable)
    QString project_json = "";
    if (filename.endsWith(".osp")) {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            project_json += line;
        }

        // Set source from project JSON
        player->SetTimelineSource(project_json.toStdString());
    } else {
        // Set source from filepath
        player->SetSource(filename.toStdString());
    }

    // Set aspect ratio of widget
    video->SetAspectRatio(player->Reader()->info.display_ratio, player->Reader()->info.pixel_ratio);

    // Play video
    player->Play();
}
