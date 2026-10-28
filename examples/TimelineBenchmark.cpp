// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Clip.h"
#include "Timeline.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

// Same workload for CPU and experimental GPU builds. Readback is optional so
// processing and the existing QImage presentation boundary can be distinguished.
int main(int argc, char** argv) {
    if (argc < 2 || argc > 6) {
        std::cerr << "Usage: openshot-timeline-benchmark VIDEO [FRAMES=120] [LAYERS=1] [READBACK=0] [DUMP.png]\n";
        return 2;
    }
    try {
        const int count = argc > 2 ? std::stoi(argv[2]) : 120;
        const int layers = argc > 3 ? std::stoi(argv[3]) : 1;
        const bool readback = argc > 4 && std::stoi(argv[4]) != 0;
        if (count < 1 || layers < 1 || layers > 16) throw std::invalid_argument("Invalid frame/layer count");
        std::vector<std::unique_ptr<openshot::Clip>> clips;
        openshot::Timeline timeline(3840, 2160, openshot::Fraction(24, 1), 48000, 2, openshot::LAYOUT_STEREO);
        timeline.SetMaxSize(854, 480);
        for (int i = 0; i < layers; ++i) {
            auto clip = std::make_unique<openshot::Clip>(argv[1]);
            clip->Layer(i);
            if (i) {
                clip->scale_x = openshot::Keyframe(0.7);
                clip->scale_y = openshot::Keyframe(0.7);
                clip->rotation = openshot::Keyframe(12.0 * i);
                clip->alpha = openshot::Keyframe(0.6);
            }
            timeline.AddClip(clip.get());
            clips.push_back(std::move(clip));
        }
        timeline.Open();
        int deferred = 0;
        std::shared_ptr<openshot::Frame> last;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 1; i <= count; ++i) {
            last = timeline.GetFrame(i);
#ifdef OPENSHOT_ENABLE_GPU_FRAME
            deferred += bool(last->GetDeferredImage());
#endif
            if (readback) last->GetImage();
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "frames=" << count << " layers=" << layers << " readback=" << readback
                  << " deferred_frames=" << deferred << " seconds=" << seconds
                  << " fps=" << count / seconds << std::endl;
        if (argc > 5 && !last->GetImage()->save(argv[5])) throw std::runtime_error("Dump failed");
        timeline.Close();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
