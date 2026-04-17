/**
 * @file
 * @brief Source file for VideoRenderer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "VideoRenderer.h"
#include <QtCore/QMetaObject>
#include <QtWidgets/QWidget>

VideoRenderer::VideoRenderer(QObject *parent)
    : QObject(parent), override_widget(nullptr)
{
}

VideoRenderer::~VideoRenderer()
{
}

/// Override QWidget which needs to be painted
void VideoRenderer::OverrideWidget(uintptr_t qwidget_address)
{
    if (override_present_connection)
        QObject::disconnect(override_present_connection);

    // re-cast QWidget pointer (long) as an actual QWidget
    override_widget = reinterpret_cast<QWidget*>(qwidget_address);
    if (!override_widget)
        return;

    override_present_connection = QObject::connect(
        this, &VideoRenderer::present,
        override_widget,
        [widget = override_widget](const QImage &image) {
            QMetaObject::invokeMethod(
                widget, "present",
                Qt::DirectConnection,
                Q_ARG(QImage, image)
            );
        },
        Qt::QueuedConnection
    );
}

void VideoRenderer::render(std::shared_ptr<QImage> image)
{
    if (!image)
        return;

    emit present(*image);
}
