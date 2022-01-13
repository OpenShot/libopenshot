/**
 * @file
 * @brief Utility templates useful when writing tests
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <ostream>
#include <iomanip>

#include <QColor>
#include <QSize>

// Stream output formatter for QColor.
//
// This allows Catch2 to display values when
// CHECK(qcolor1 == qcolor2) comparisons fail.
// No setup is required, just include this header.
std::ostream& operator<<(std::ostream& os, const QColor& value)
{
    os << std::unitbuf;  // Don't auto-flush on destruction
    os << "QColor(" << value.red() << ", " << value.green() << ", "
       << value.blue() << ", " << value.alpha() << ")";
    return os;
}

// Same for QSize
std::ostream& operator<<(std::ostream& os, const QSize& size)
{
    os << std::unitbuf;
    os << "QSize(" << size.width() << ", " << size.height() << ")";
    return os;
}
