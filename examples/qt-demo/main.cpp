/**
 * @file
 * @brief Demo Qt application to test the QtPlayer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "Qt/PlayerDemo.h"
#include "ZmqLogger.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // Enable logging for openshot-player since this is primarily used for
    // profiling and debugging video playback issues.
    openshot::ZmqLogger::Instance()->Enable(true);
    openshot::ZmqLogger::Instance()->Path("./player.log");

    QApplication app(argc, argv);
    PlayerDemo demo;
    demo.show();
    return app.exec();
}
