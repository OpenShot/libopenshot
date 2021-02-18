/**
 * @file
 * @brief Header file for demo application for QtPlayer class
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

#ifndef OPENSHOT_PLAYER_DEMO_H
#define OPENSHOT_PLAYER_DEMO_H

#include <QObject>
#include <QWidget>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QMenuBar>

#include "VideoRenderWidget.h"

// Define the QtPlayer without including it (due to build issues with Qt moc / Qt macros)
namespace openshot
{
    class QtPlayer;
}

class PlayerDemo : public QWidget
{
    Q_OBJECT

public:
    PlayerDemo(QWidget *parent = 0);
    ~PlayerDemo();

protected:
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;
    void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;

private slots:
    void open(bool checked);

private:
    QVBoxLayout *vbox;
    QMenuBar *menu;
    VideoRenderWidget *video;
    openshot::QtPlayer *player;
};

#endif // OPENSHOT_PLAYER_H
