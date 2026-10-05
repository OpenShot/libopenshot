// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later
// Optimized native workload: compile unchanged against baseline and candidate.
#include "Qt/VideoCacheThread.h"
#include "Timeline.h"
#include "Frame.h"
#include "CacheBase.h"
#include "Settings.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <sys/resource.h>
using namespace openshot;
using Clock = std::chrono::steady_clock;
class SyntheticTimeline : public Timeline {
public:
    std::mutex decode;
    int profile;
    SyntheticTimeline(int p) : Timeline(160,90,Fraction(30,1),48000,2,LAYOUT_STEREO), profile(p) {
        GetCache()->SetMaxBytes(16 * 1024 * 1024);
    }
    std::shared_ptr<Frame> GetFrame(int64_t n) override {
        if (auto cached = GetCache()->GetFrame(n)) return cached;
        std::lock_guard<std::mutex> lock(decode);
        if (auto cached = GetCache()->GetFrame(n)) return cached;
        const int ms = profile == 0 ? 2 : profile == 1 ? (n % 7 == 0 ? 45 : 2) : 55;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        auto frame = std::make_shared<Frame>(n,160,90,"black",1600,2);
        GetCache()->Add(frame);
        return frame;
    }
};
int main(int argc, char** argv) {
    const bool honor_gate = argc > 1;
    const int selected_profile = argc > 2 ? std::atoi(argv[2]) : -1;
    Settings::Instance()->ENABLE_PLAYBACK_CACHING = true;
    Settings::Instance()->VIDEO_CACHE_MIN_PREROLL_FRAMES = 30;
    Settings::Instance()->VIDEO_CACHE_MAX_PREROLL_FRAMES = 60;
    Settings::Instance()->VIDEO_CACHE_MAX_FRAMES = 64;
    for (int profile = 0; profile < 3; ++profile) {
        if (selected_profile >= 0 && selected_profile != profile) continue;
        std::vector<double> start_ms, frame_work_ms;
        int underruns = 0, holds = 0;
        int64_t peak_bytes = 0;
        for (int run = 0; run < 8; ++run) {
            SyntheticTimeline timeline(profile);
            VideoCacheThread worker;
            worker.Reader(&timeline);
            worker.setSpeed(1);
            const auto start = Clock::now();
            worker.StartThread();
            while (!worker.isReady()) {
                if (Clock::now() - start > std::chrono::seconds(8)) return 2;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            start_ms.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            for (int64_t n = 1; n <= 45; ++n) {
                const auto frame_start = Clock::now();
                if (!worker.isReady()) ++holds;
                if (honor_gate) {
                    while (!worker.isReady()) {
                        if (Clock::now() - frame_start > std::chrono::seconds(8)) return 3;
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
                if (!timeline.GetCache()->Contains(n + 1)) ++underruns;
                timeline.GetFrame(n + 1); // Established synchronous video fallback.
                worker.NotifyPlaybackPosition(n + 1);
                frame_work_ms.push_back(std::chrono::duration<double,std::milli>(Clock::now()-frame_start).count());
                peak_bytes = std::max(peak_bytes, timeline.GetCache()->GetBytes());
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
            }
            worker.StopThread(-1);
        }
        std::sort(start_ms.begin(),start_ms.end());
        std::sort(frame_work_ms.begin(),frame_work_ms.end());
        struct rusage usage{};
        getrusage(RUSAGE_SELF,&usage);
        std::cout << "honor_gate=" << honor_gate << " profile=" << profile << " n=8 startup_ms median=" << start_ms[4]
                  << " p95=" << start_ms[7] << " max=" << start_ms.back()
                  << " next_frame_misses=" << underruns << "/360 gate_holds=" << holds
                  << " frame_work_ms_p95=" << frame_work_ms[342] << " frame_work_ms_max=" << frame_work_ms.back()
                  << " cache_peak_bytes=" << peak_bytes << " process_peak_rss_kib=" << usage.ru_maxrss << std::endl;
    }
}
