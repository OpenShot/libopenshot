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


VideoRenderer::VideoRenderer(QObject *parent)
    : QObject(parent)
{
}

VideoRenderer::~VideoRenderer()
{
}

/// Override QWidget which needs to be painted
void VideoRenderer::OverrideWidget(int64_t qwidget_address)
{
	// re-cast QWidget pointer (long) as an actual QWidget
	override_widget = reinterpret_cast<QWidget*>(qwidget_address);

}

void VideoRenderer::render(std::shared_ptr<QImage> image)
{
    if (image)
        emit present(*image);
}
