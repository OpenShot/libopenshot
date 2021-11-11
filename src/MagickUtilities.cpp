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

    // Get the pixels from the frame image
    const QRgb *tmpBits = (const QRgb*)image->constBits();

    // Create new image object, and fill with pixel data
    auto magick_image = std::make_shared<Magick::Image>(
        image->width(), image->height(),
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

    MagickCore::ExceptionInfo exception;
    // TODO: Actually do something, if we get an exception here
    MagickCore::ExportImagePixels(
        image->constImage(), 0, 0,
        image->columns(), image->rows(),
        "RGBA", Magick::CharPixel,
        qbuffer, &exception);

    auto qimage = std::make_shared<QImage>(
        qbuffer, image->columns(), image->rows(),
        image->columns() * BPP,
        QImage::Format_RGBA8888_Premultiplied,
        (QImageCleanupFunction) &openshot::cleanUpBuffer,
        (void*) qbuffer);
    return qimage;
}

#endif  // USE_IMAGEMAGICK
