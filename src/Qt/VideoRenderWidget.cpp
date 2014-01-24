/**
 *  @file VideoRenderWidget.cpp
 */

#include "../../include/Qt/VideoRenderWidget.h"
#include "../../include/Qt/VideoRenderer.h"
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

VideoRenderWidget::VideoRenderWidget(QWidget *parent)
    : QWidget(parent)
    , renderer(new VideoRenderer(this))
{
    QPalette p = palette();
    p.setColor(QPalette::Window, Qt::black);
    setPalette(p);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

VideoRenderWidget::~VideoRenderWidget()
{
}

VideoRenderer *VideoRenderWidget::GetRenderer() const
{
    return renderer;
}

void VideoRenderWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if (testAttribute(Qt::WA_OpaquePaintEvent)) {
        painter.fillRect(event->rect(), palette().window());
    }

    renderer->paint(&painter, event->rect());
}
