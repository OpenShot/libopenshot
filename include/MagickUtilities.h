/**
 * @file
 * @brief Header file for MagickUtilities (IM6/IM7 compatibility overlay)
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_MAGICK_UTILITIES_H
#define OPENSHOT_MAGICK_UTILITIES_H

#ifdef USE_IMAGEMAGICK

    #include "Magick++.h"

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
