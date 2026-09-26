// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_PREVIEW_SIZE_H
#define OPENSHOT_PREVIEW_SIZE_H

#include <algorithm>
#include <QSize>

namespace openshot {
// Apply only to reduced previews, after fitting the aspect ratio. Four RGBA
// pixels occupy 16 bytes. Align both axes so quarter-turn orientation also
// produces aligned rows. Tiny previews have a minimum size of 4x4.
inline QSize AlignPreviewSize(const QSize& size) {
    return QSize(std::max(4, size.width() / 4 * 4),
                 std::max(4, size.height() / 4 * 4));
}
}

#endif
