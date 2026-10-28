// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_FRAME_IMAGE_H
#define OPENSHOT_FRAME_IMAGE_H

#include <cstdint>
#include <QImage>

namespace openshot {

/// Immutable image storage, including a hardware surface with deferred readback.
/// Implementations own their resources and synchronization. Materialize must be
/// safe for concurrent callers and return independent/COW CPU image storage.
class FrameImage {
public:
    virtual ~FrameImage() = default;
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    /// Conservative resident allocation size, for cache budgeting.
    virtual int64_t GetBytes() const = 0;
    virtual QImage Materialize() const = 0;
};

} // namespace openshot
#endif
