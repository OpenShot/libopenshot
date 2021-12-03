/**
 * @file
 * @brief Header file for Video RendererWidget class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_VIDEO_RENDERER_WIDGET_H
#define OPENSHOT_VIDEO_RENDERER_WIDGET_H

#include "../Fraction.h"
#include "VideoRenderer.h"

#include <QWidget>
#include <QImage>
#include <QPaintEvent>
#include <QRect>

class VideoRenderWidget : public QWidget
{
    Q_OBJECT

private:
    VideoRenderer *renderer;
    QImage image;
    openshot::Fraction aspect_ratio;
    openshot::Fraction pixel_ratio;

public:
    VideoRenderWidget(QWidget *parent = 0);
    ~VideoRenderWidget();

    VideoRenderer *GetRenderer() const;
    void SetAspectRatio(openshot::Fraction new_aspect_ratio, openshot::Fraction new_pixel_ratio);

protected:
    void paintEvent(QPaintEvent *event);

    QRect centeredViewport(int width, int height);

private slots:
    void present(const QImage &image);

};

#endif // OPENSHOT_VIDEO_RENDERER_WIDGET_H
