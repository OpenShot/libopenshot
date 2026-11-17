// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_MESH_WARP_H
#define OPENSHOT_MESH_WARP_H
#include <QPointF>
#include <algorithm>
#include <cmath>
#include <utility>
namespace openshot {
namespace mesh_warp {
/// Integer coverage shared by CPU rasterization and Vulkan. Computing row
/// spans once in double precision avoids cracks or expanded silhouettes from
/// float barycentric comparisons on the GPU, including at HD/4K resolutions.
inline std::pair<int,int> Span(QPointF a, QPointF b, QPointF c, int y, int left, int right) {
    const QPointF e=b-a, f=c-a;
    const double det=e.x()*f.y()-e.y()*f.x();
    const double dx=0.5-a.x(), dy=y+0.5-a.y();
    const double bs=f.y()/det, cs=-e.y()/det;
    const double b0=(dx*f.y()-dy*f.x())/det, c0=(e.x()*dy-e.y()*dx)/det;
    double low=left, high=right;
    auto clip=[&](double slope, double offset) {
        if (slope>0) low=std::max(low,(-1e-9-offset)/slope);
        else if (slope<0) high=std::min(high,(-1e-9-offset)/slope);
        else if (offset < -1e-9) high=low-1;
    };
    clip(bs,b0); clip(cs,c0); clip(-bs-cs,1-b0-c0);
    if (low>high) return {0,-1};
    return {int(std::ceil(low)),int(std::floor(high))};
}
}
}
#endif
