 /****************************************************************************
 **
 ** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 ** All rights reserved.
 ** Contact: Nokia Corporation (qt-info@nokia.com)
 **
 ** This file is part of the examples of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:BSD$
 ** You may use this file under the terms of the BSD license as follows:
 **
 ** "Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are
 ** met:
 **   * Redistributions of source code must retain the above copyright
 **     notice, this list of conditions and the following disclaimer.
 **   * Redistributions in binary form must reproduce the above copyright
 **     notice, this list of conditions and the following disclaimer in
 **     the documentation and/or other materials provided with the
 **     distribution.
 **   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
 **     the names of its contributors may be used to endorse or promote
 **     products derived from this software without specific prior written
 **     permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 ** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 ** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

 #include "../../include/Qt/videowidgetsurface.h"

 #include <QtMultimedia>

 VideoWidgetSurface::VideoWidgetSurface(QWidget *widget, QObject *parent)
     : QAbstractVideoSurface(parent)
     , widget(widget)
     , imageFormat(QImage::Format_Invalid)
 {
 }

 QList<QVideoFrame::PixelFormat> VideoWidgetSurface::supportedPixelFormats(
         QAbstractVideoBuffer::HandleType handleType) const
 {
     if (handleType == QAbstractVideoBuffer::NoHandle) {
         return QList<QVideoFrame::PixelFormat>()
                 << QVideoFrame::Format_RGB32
                 << QVideoFrame::Format_ARGB32
                 << QVideoFrame::Format_ARGB32_Premultiplied
                 << QVideoFrame::Format_RGB565
                 << QVideoFrame::Format_RGB555;
     } else {
         return QList<QVideoFrame::PixelFormat>();
     }
 }

 bool VideoWidgetSurface::isFormatSupported(
         const QVideoSurfaceFormat &format, QVideoSurfaceFormat *similar) const
 {
     Q_UNUSED(similar);

     const QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat());
     const QSize size = format.frameSize();

     return imageFormat != QImage::Format_Invalid
             && !size.isEmpty()
             && format.handleType() == QAbstractVideoBuffer::NoHandle;
 }

 bool VideoWidgetSurface::start(const QVideoSurfaceFormat &format)
 {
     const QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat());
     const QSize size = format.frameSize();

     if (imageFormat != QImage::Format_Invalid && !size.isEmpty()) {
         this->imageFormat = imageFormat;
         imageSize = size;
         sourceRect = format.viewport();

         QAbstractVideoSurface::start(format);

         widget->updateGeometry();
         updateVideoRect();

         return true;
     } else {
         return false;
     }
 }

 void VideoWidgetSurface::stop()
 {
     currentFrame = QVideoFrame();
     targetRect = QRect();

     QAbstractVideoSurface::stop();

     widget->update();
 }

 bool VideoWidgetSurface::present(const QVideoFrame &frame)
 {
     if (surfaceFormat().pixelFormat() != frame.pixelFormat()
             || surfaceFormat().frameSize() != frame.size()) {
         setError(IncorrectFormatError);
         stop();

         return false;
     } else {
    	 cout << "Paint new frame!" << endl;
         currentFrame = frame;

         widget->repaint(targetRect);

         return true;
     }
 }

 void VideoWidgetSurface::updateVideoRect()
 {
	 cout << "update rect" << endl;
     //QSize size = surfaceFormat().sizeHint();
	 QSize size(1280, 720);
     //size.scale(widget->size().boundedTo(size), Qt::KeepAspectRatio);

     targetRect = QRect(QPoint(0, 0), size);
     targetRect.moveCenter(widget->rect().center());

 }

 void VideoWidgetSurface::paint(QPainter *painter)
 {
     if (currentFrame.map(QAbstractVideoBuffer::ReadOnly)) {
         const QTransform oldTransform = painter->transform();

         if (surfaceFormat().scanLineDirection() == QVideoSurfaceFormat::BottomToTop) {
            painter->scale(1, -1);
            painter->translate(0, -widget->height());
         }

         QImage image(
                 currentFrame.bits(),
                 currentFrame.width(),
                 currentFrame.height(),
                 currentFrame.bytesPerLine(),
                 imageFormat);

         painter->drawImage(targetRect, image, sourceRect);

         painter->setTransform(oldTransform);

         currentFrame.unmap();
     }
 }
