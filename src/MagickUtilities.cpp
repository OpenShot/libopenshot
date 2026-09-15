/**
 * @file
 * @brief Implementation for MagickUtilities (conversions)
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 */

// Copyright (c) 2008-2021 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifdef USE_IMAGEMAGICK

#include "MagickUtilities.h"
#include "QtUtilities.h"

#include <QImage>

// Get pointer to Magick::Image conversion of a QImage
std::shared_ptr<Magick::Image>
openshot::QImage2Magick(std::shared_ptr<QImage> image)
{
    // Check for blank image
    if (!image || image->isNull())
        return nullptr;

    // Export a straight-alpha RGBA pixel buffer. Many libopenshot frames are
    // stored in Qt's premultiplied format, which is convenient for compositing
    // but not what ImageMagick expects when importing raw RGBA bytes.
    const QImage rgba_image = image->convertToFormat(QImage::Format_RGBA8888);
    const unsigned char *tmpBits = rgba_image.constBits();

    // Create new image object, and fill with pixel data
    auto magick_image = std::make_shared<Magick::Image>(
        rgba_image.width(), rgba_image.height(),
        "RGBA", Magick::CharPixel, tmpBits);

    // Give image a transparent background color
    magick_image->backgroundColor(Magick::Color("none"));
    magick_image->virtualPixelMethod(
        Magick::TransparentVirtualPixelMethod);
    MAGICK_IMAGE_ALPHA(magick_image, true);

    return magick_image;
}

// Get pointer to QImage conversion of a Magick::Image
std::shared_ptr<QImage>
openshot::Magick2QImage(std::shared_ptr<Magick::Image> image)
{
    if (!image)
        return nullptr;

    const int BPP = 4;
    const std::size_t size = image->columns() * image->rows() * BPP;

    auto* qbuffer = new unsigned char[size]();

    MagickCore::ExceptionInfo* exception = MagickCore::AcquireExceptionInfo();
    if (!exception) {
        delete[] qbuffer;
        return nullptr;
    }
    const auto export_ok = MagickCore::ExportImagePixels(
        image->constImage(), 0, 0,
        image->columns(), image->rows(),
        "RGBA", Magick::CharPixel,
        qbuffer, exception);
    const bool export_failed =
        (export_ok == Magick::MagickFalse) ||
        (exception->severity != MagickCore::UndefinedException);
    exception = MagickCore::DestroyExceptionInfo(exception);
    if (export_failed) {
        delete[] qbuffer;
        return nullptr;
    }

    auto qimage = std::make_shared<QImage>(
        qbuffer, image->columns(), image->rows(),
        image->columns() * BPP,
        QImage::Format_RGBA8888,
        (QImageCleanupFunction) &openshot::cleanUpArrayBuffer,
        (void*) qbuffer);
    return qimage;
}

#endif  // USE_IMAGEMAGICK
