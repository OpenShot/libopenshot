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
#include <QVariant>
#include <QtWidgets/QWidget>

VideoRenderer::VideoRenderer(QObject *parent)
    : QObject(parent), override_widget(nullptr)
{
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    qRegisterMetaType<std::shared_ptr<openshot::Frame>>();
#endif
}

VideoRenderer::~VideoRenderer()
{
}

/// Override QWidget which needs to be painted
void VideoRenderer::OverrideWidget(uintptr_t qwidget_address)
{
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    frame_presenter.store(false);
#endif
    if (override_present_connection)
        QObject::disconnect(override_present_connection);

    // re-cast QWidget pointer (long) as an actual QWidget
    override_widget = reinterpret_cast<QWidget*>(qwidget_address);
    if (!override_widget)
        return;

#ifdef OPENSHOT_ENABLE_GPU_FRAME
    QObject* presenter_object = override_widget;
    auto presenter = dynamic_cast<FramePresenter*>(presenter_object);
    if (!presenter) {
        presenter_object = override_widget->property("openshot_gpu_presenter").value<QObject*>();
        presenter = dynamic_cast<FramePresenter*>(presenter_object);
    }
    if (presenter) {
        override_present_connection = QObject::connect(
            this, &VideoRenderer::presentFrame, presenter_object,
            [presenter](std::shared_ptr<openshot::Frame> frame) {
                presenter->PresentFrame(frame);
            }, Qt::QueuedConnection);
        frame_presenter.store(true);
        return;
    }
#endif

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

#ifdef OPENSHOT_ENABLE_GPU_FRAME
void VideoRenderer::renderFrame(const std::shared_ptr<openshot::Frame>& frame)
{
    if (frame_presenter.load())
        emit presentFrame(frame);
    else
        openshot::RendererBase::renderFrame(frame);
}
#endif
