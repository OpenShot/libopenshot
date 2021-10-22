/**
 * @file
 * @brief Header file for MagickUtilities (IM6/IM7 compatibility overlay)
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_MAGICK_UTILITIES_H
#define OPENSHOT_MAGICK_UTILITIES_H

#ifdef USE_IMAGEMAGICK

// Exclude a warning message with IM6 headers
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
    #include "Magick++.h"
#pragma GCC diagnostic pop

    // Determine ImageMagick version, as IM7 isn't fully
    // backwards compatible
    #ifndef NEW_MAGICK
	   #define NEW_MAGICK (MagickLibVersion >= 0x700)
	#endif

    // IM7: <Magick::Image>->alpha(bool)
    // IM6: <Magick::Image>->matte(bool)
    #if NEW_MAGICK
        #define MAGICK_IMAGE_ALPHA(im, a) im->alpha((a))
    #else
        #define MAGICK_IMAGE_ALPHA(im, a) im->matte((a))
    #endif

    // IM7: vector<Magick::Drawable>
    // IM6: list<Magick::Drawable>
    // (both have the push_back() method which is all we use)
    #if NEW_MAGICK
        #define MAGICK_DRAWABLE std::vector<Magick::Drawable>
    #else
        #define MAGICK_DRAWABLE std::list<Magick::Drawable>
    #endif

#endif
#endif
