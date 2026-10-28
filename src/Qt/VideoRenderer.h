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
#include <atomic>


class QPainter;
class QWidget;

#ifdef OPENSHOT_ENABLE_GPU_FRAME
/// Implement on a native QWidget to receive retained frames on the GUI thread.
/// The presenter owns presentation synchronization and may retain the frame.
class FramePresenter {
public:
    virtual ~FramePresenter() = default;
    virtual void PresentFrame(const std::shared_ptr<openshot::Frame>& frame) = 0;
};
Q_DECLARE_METATYPE(std::shared_ptr<openshot::Frame>)
#endif

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
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    void presentFrame(std::shared_ptr<openshot::Frame> frame);
#endif

protected:
    //void render(openshot::OSPixelFormat format, int width, int height, int bytesPerLine, unsigned char *data);
    void render(std::shared_ptr<QImage> image);
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    void renderFrame(const std::shared_ptr<openshot::Frame>& frame) override;
#endif

private slots:

private:
    QWidget* override_widget;
    QMetaObject::Connection override_present_connection;
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    std::atomic<bool> frame_presenter{false};
#endif
};

#endif //OPENSHOT_VIDEO_RENDERER_H
