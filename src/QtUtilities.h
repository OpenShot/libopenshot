/**
 * @file
 * @brief Header file for QtUtilities (compatibiity overlay)
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 */

// Copyright (c) 2008-2020 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_QT_UTILITIES_H
#define OPENSHOT_QT_UTILITIES_H

#include <Qt>
#include <QTextStream>

// Fix Qt::endl for older Qt versions
// From: https://bugreports.qt.io/browse/QTBUG-82680
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
  using TextStreamFunction = QTextStream& (*)(QTextStream&);
  constexpr TextStreamFunction endl = ::endl;
}
#endif


namespace openshot {
    // Clean up buffer after QImage is deleted
    static inline void cleanUpBuffer(void *info)
    {
        if (!info)
            return;
        // Remove buffer since QImage tells us to
        auto* qbuffer = reinterpret_cast<unsigned char*>(info);
        delete[] qbuffer;
    }
}  // namespace

#endif // OPENSHOT_QT_UTILITIES_H
