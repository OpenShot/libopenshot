/**
 * @file
 * @brief Source file for Fraction class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FPS.h"
#include <cmath>

using namespace openshot;

int64_t openshot::FPS::frame(double time) const {
    return static_cast<int64_t>(*this * time) + 1;
}

double openshot::FPS::time(int64_t frame) const {
    return static_cast<double>(frame - 1) / *this;
}

int64_t openshot::FPS::sample(int64_t frame, int sample_rate) const {
    const auto spf = static_cast<double>(sample_rate) * this->Reciprocal();
    return frame > 1
        ? std::floor(static_cast<double>(frame - 1) * spf)
        : 0;
}
