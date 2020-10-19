/**
 * @file
 * @brief Header file for QtUtilities (compatibiity overlay)
 * @author FeRD (Frank Dana) <ferdnyc@gmail.com>
 */

/* LICENSE
 *
 * Copyright (c) 2008-2020 OpenShot Studios, LLC
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

#endif // OPENSHOT_QT_UTILITIES_H
