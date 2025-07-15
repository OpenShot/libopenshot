/**
 * @file
 * @brief Header file for QtUtilities (compatibiity overlay)
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_QT_UTILITIES_H
#define OPENSHOT_QT_UTILITIES_H

#include <iostream>
#include <Qt>
#include <QTextStream>
#include <cstdlib>

// Fix Qt::endl for older Qt versions
// From: https://bugreports.qt.io/browse/QTBUG-82680
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
  using TextStreamFunction = QTextStream& (*)(QTextStream&);
  constexpr TextStreamFunction endl = ::endl;
}
#endif


namespace openshot {

    // Cross-platform aligned free function
    inline void aligned_free(void* ptr)
    {
#if defined(_WIN32)
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }

    // Clean up buffer after QImage is deleted
    static inline void cleanUpBuffer(void *info)
    {
        if (!info)
            return;

        // Free the aligned memory buffer
        aligned_free(info);
    }
}  // namespace

#endif // OPENSHOT_QT_UTILITIES_H
