/**
 * @file
 * @brief Example application showing how to attach VideoCacheThread to an FFmpegReader
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <chrono>
#include <iostream>
#include <memory>
#include "Frame.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Timeline.h"
#include "Qt/VideoCacheThread.h"    // <— your new header

using namespace openshot;

int main(int argc, char* argv[]) {


    // 1) Open the FFmpegReader as usual
    const char* input_path = "/home/jonathan/Downloads/openshot-testing/sintel_trailer-720p.mp4";
    FFmpegReader reader(input_path);
    reader.Open();

    const int64_t total_frames = reader.info.video_length;
    std::cout << "Total frames: " << total_frames << "\n";



    Timeline timeline(reader.info.width, reader.info.height, reader.info.fps, reader.info.sample_rate, reader.info.channels, reader.info.channel_layout);
    Clip c1(&reader);
    timeline.AddClip(&c1);
    timeline.Open();
    timeline.DisplayInfo();


    // 2) Construct a VideoCacheThread around 'reader' and start its background loop
    //    (VideoCacheThread inherits juce::Thread)
    std::shared_ptr<VideoCacheThread> cache = std::make_shared<VideoCacheThread>();
    cache->Reader(&timeline);    // attaches the FFmpegReader and internally calls Play()
    cache->StartThread();      // juce::Thread method, begins run()

    // 3) Set up the writer exactly as before
    FFmpegWriter writer("/home/jonathan/Downloads/performance‐cachetest.mp4");
    writer.SetAudioOptions("aac", 48000, 192000);
    writer.SetVideoOptions("libx264", 1280, 720, Fraction(30, 1), 5000000);
    writer.Open();

    // 4) Forward pass: for each frame 1…N, tell the cache thread to seek to that frame,
    //    then immediately call cache->GetFrame(frame), which will block only if that frame
    //    hasn’t been decoded into the cache yet.
    auto t0 = std::chrono::high_resolution_clock::now();
    cache->setSpeed(1);
    for (int64_t f = 1; f <= total_frames; ++f) {
        float pct = (float(f) / total_frames) * 100.0f;
        std::cout << "Forward: requesting frame " << f << " (" << pct << "%)\n";

        cache->Seek(f);                   // signal “I need frame f now (and please prefetch f+1, f+2, …)”
        std::shared_ptr<Frame> framePtr = timeline.GetFrame(f);
        writer.WriteFrame(framePtr);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    auto forward_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // 5) Backward pass: same idea in reverse
    auto t2 = std::chrono::high_resolution_clock::now();
    cache->setSpeed(-1);
    for (int64_t f = total_frames; f >= 1; --f) {
        float pct = (float(total_frames - f + 1) / total_frames) * 100.0f;
        std::cout << "Backward: requesting frame " << f << " (" << pct << "%)\n";

        cache->Seek(f);
        std::shared_ptr<Frame> framePtr = timeline.GetFrame(f);
        writer.WriteFrame(framePtr);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    auto backward_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

    std::cout << "\nForward pass elapsed:  "  << forward_ms  << " ms\n";
    std::cout << "Backward pass elapsed: " << backward_ms << " ms\n";

    // 6) Shut down the cache thread, close everything
    cache->StopThread(10000);  // politely tells run() to exit, waits up to 10s
    reader.Close();
    writer.Close();
    timeline.Close();
    return 0;
}
