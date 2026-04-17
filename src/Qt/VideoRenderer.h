/**
 * @file
 * @brief Header file for Video Renderer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_VIDEO_RENDERER_H
#define OPENSHOT_VIDEO_RENDERER_H

#include "../RendererBase.h"
#include <cstdint>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtGui/QImage>
#include <memory>


class QPainter;
class QWidget;

class VideoRenderer : public QObject, public openshot::RendererBase
{
    Q_OBJECT

public:
    VideoRenderer(QObject *parent = 0);
    ~VideoRenderer();

    /// Override QWidget which needs to be painted
    void OverrideWidget(uintptr_t qwidget_address);

signals:
    void present(const QImage &image);

protected:
    //void render(openshot::OSPixelFormat format, int width, int height, int bytesPerLine, unsigned char *data);
    void render(std::shared_ptr<QImage> image);

private slots:

private:
    QWidget* override_widget;
    QMetaObject::Connection override_present_connection;
};

#endif //OPENSHOT_VIDEO_RENDERER_H
