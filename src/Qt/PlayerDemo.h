/**
 * @file
 * @brief Header file for demo application for QtPlayer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

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
