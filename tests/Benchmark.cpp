/**
 * @file
 * @brief Benchmark executable for core libopenshot operations
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @ref License
 */
// Copyright (c) 2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

extern "C" {
#include <libavutil/log.h>
}

#include "BenchmarkOptions.h"
#include "Clip.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Frame.h"
#include "Fraction.h"
#include "FrameMapper.h"
#ifdef USE_IMAGEMAGICK
#include "ImageReader.h"
#else
#include "QtImageReader.h"
#endif
#include "ReaderBase.h"
#include "Settings.h"
#include "Timeline.h"
// Each effect is guarded so the benchmark compiles against older libopenshot
// builds that predate a given effect — missing effects are silently skipped.
#if __has_include("effects/AudioVisualization.h")
#  include "effects/AudioVisualization.h"
#  define OPENSHOT_HAS_AUDIOVISUALIZATION
#endif
#if __has_include("effects/BeatSync.h")
#  include "effects/BeatSync.h"
#  define OPENSHOT_HAS_BEATSYNC
#endif
#if __has_include("effects/Brightness.h")
#  include "effects/Brightness.h"
#  define OPENSHOT_HAS_BRIGHTNESS
#endif
#if __has_include("effects/ChromaKey.h")
#  include "effects/ChromaKey.h"
#  define OPENSHOT_HAS_CHROMAKEY
#endif
#if __has_include("effects/ColorGrade.h")
#  include "effects/ColorGrade.h"
#  define OPENSHOT_HAS_COLORGRADE
#endif
#if __has_include("effects/Crop.h")
#  include "effects/Crop.h"
#  define OPENSHOT_HAS_CROP
#endif
#if __has_include("effects/FilmGrain.h")
#  include "effects/FilmGrain.h"
#  define OPENSHOT_HAS_FILMGRAIN
#endif
#if __has_include("effects/Glow.h")
#  include "effects/Glow.h"
#  define OPENSHOT_HAS_GLOW
#endif
#if __has_include("effects/Mask.h")
#  include "effects/Mask.h"
#  define OPENSHOT_HAS_MASK
#endif
#if __has_include("effects/Saturation.h")
#  include "effects/Saturation.h"
#  define OPENSHOT_HAS_SATURATION
#endif
#if __has_include("effects/Shadow.h")
#  include "effects/Shadow.h"
#  define OPENSHOT_HAS_SHADOW
#endif
#if __has_include("effects/Blur.h")
#  include "effects/Blur.h"
#  define OPENSHOT_HAS_BLUR
#endif
#if __has_include("effects/Sharpen.h")
#  include "effects/Sharpen.h"
#  define OPENSHOT_HAS_SHARPEN
#endif
#if __has_include("effects/DenoiseImage.h")
#  include "effects/DenoiseImage.h"
#  define OPENSHOT_HAS_DENOISEIMAGE
#endif
#if __has_include("effects/Hue.h")
#  include "effects/Hue.h"
#  define OPENSHOT_HAS_HUE
#endif
#if __has_include("effects/Negate.h")
#  include "effects/Negate.h"
#  define OPENSHOT_HAS_NEGATE
#endif
#if __has_include("effects/Pixelate.h")
#  include "effects/Pixelate.h"
#  define OPENSHOT_HAS_PIXELATE
#endif
#if __has_include("effects/Wave.h")
#  include "effects/Wave.h"
#  define OPENSHOT_HAS_WAVE
#endif
#if __has_include("effects/Caption.h")
#  include "effects/Caption.h"
#  define OPENSHOT_HAS_CAPTION
#endif
#if __has_include("effects/Timer.h")
#  include "effects/Timer.h"
#  define OPENSHOT_HAS_TIMER
#endif
#if __has_include("effects/Displace.h")
#  include "effects/Displace.h"
#  define OPENSHOT_HAS_DISPLACE
#endif
#if __has_include("FrameScope.h")
#  include "FrameScope.h"
#  define OPENSHOT_HAS_FRAMESCOPE
#endif

#include <QApplication>
#include <QImage>

using namespace openshot;
using namespace std;

using Clock = chrono::steady_clock;
using TrialResult = pair<int64_t, double>;  // (frames or JSON updates, elapsed_seconds)
using TrialFunc = function<TrialResult()>;
using Trial = pair<string, TrialFunc>;

// Frame 241 = 10 seconds into a 24 fps source (frames are 1-based: 24*10 + 1).
// The seek to this frame is included in the timed window — it's real cost OpenShot pays.
constexpr int64_t START_FRAME = 241;
constexpr int64_t BENCH_FRAMES = 120;

struct BenchmarkRecord {
    string name;
    double fps;
};

// Each trial is run this many times; the median is reported.
// The first (cold-cache) run is naturally the low outlier and is discarded by the median,
// giving stable warm-state numbers without an explicit warmup pass.
constexpr int TRIAL_RUNS = 3;

static const char* spinner_frames[] = {"⠋","⠙","⠸","⠴","⠦","⠇"};
static int spinner_index = 0;

static BenchmarkRecord run_trial(const string& name, TrialFunc func) {
    vector<double> samples;
    samples.reserve(TRIAL_RUNS);
    for (int run = 1; run <= TRIAL_RUNS; ++run) {
        if (isatty(STDOUT_FILENO)) {
            cout << "\r" << spinner_frames[spinner_index % 6] << " "
                 << left << setw(44) << (name + "...")
                 << " [" << run << "/" << TRIAL_RUNS << "]" << flush;
            spinner_index++;
        }
        TrialResult r = func();
        samples.push_back(r.first / r.second);
    }
    sort(samples.begin(), samples.end());
    return {name, samples[TRIAL_RUNS / 2]};  // median
}

static void print_results(const vector<BenchmarkRecord>& records) {
    if (records.empty()) return;

    double max_fps = 0.0;
    size_t max_name = 5;  // at least "Trial"
    for (const auto& rec : records) {
        max_fps = std::max(max_fps, rec.fps);
        max_name = std::max(max_name, rec.name.size());
    }

    const int bar_width = 24;
    const double sqrt_max = std::sqrt(max_fps);

    auto make_bar = [&](double fps) {
        int n = static_cast<int>(std::sqrt(fps) / sqrt_max * bar_width + 0.5);
        string bar;
        bar.reserve(static_cast<size_t>(n) * 3);
        for (int i = 0; i < n; ++i)
            bar += "\xe2\x96\x88";  // UTF-8 █
        return bar;
    };

    cout << "| " << left  << setw(static_cast<int>(max_name)) << "Trial"
         << " | "  << right << setw(8) << "Ops/s"
         << " | Chart |\n";
    cout << "|:" << string(max_name, '-')
         << "-|" << string(8, '-') << ":|:"
         << string(bar_width, '-') << "-|\n";
    for (const auto& rec : records) {
        cout << "| " << left  << setw(static_cast<int>(max_name)) << rec.name
             << " | " << fixed << setprecision(1) << right << setw(8) << rec.fps
             << " | " << make_bar(rec.fps) << " |\n";
    }
}

// Time a forward sequential read of BENCH_FRAMES frames starting at START_FRAME.
// The seek to START_FRAME is included — it reflects real scrubbing cost.
static TrialResult timed_read(ReaderBase& r) {
    auto t0 = Clock::now();
    for (int64_t i = 0; i < BENCH_FRAMES; ++i)
        r.GetFrame(START_FRAME + i);
    return {BENCH_FRAMES, chrono::duration<double>(Clock::now() - t0).count()};
}

// Model dragging a clip's X/Y transform: each edit sends the complete, growing
// curves, as openshot-qt does. Prepare strings outside the measured interval so
// this measures ApplyJsonDiff (including parsing), not JSON generation or render.
static TrialResult timed_transform_json(const string& image, int existing_keys) {
    constexpr int edits = 100;
    Clip clip(image);
    clip.Id("json-benchmark-clip");
    clip.Layer(5);
    clip.End((existing_keys + edits + 24) / 24.0);
    Timeline timeline(1280, 720, Fraction(24, 1), 44100, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();

    Json::Value diff(Json::arrayValue);
    Json::Value change(Json::objectValue);
    change["type"] = "update";
    change["key"].append("clips");
    Json::Value id(Json::objectValue);
    id["id"] = clip.Id();
    change["key"].append(id);
    diff.append(change);
    auto& value = diff[0]["value"];
    value["location_x"]["Points"] = Json::Value(Json::arrayValue);
    value["location_y"]["Points"] = Json::Value(Json::arrayValue);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    vector<string> payloads;
    payloads.reserve(edits);
    for (int frame = 1; frame <= existing_keys + edits; ++frame) {
        // Include Bezier handles, matching real project keyframes.
        value["location_x"]["Points"].append(Point(frame, sin(frame * 0.05) * 0.4, BEZIER).JsonValue());
        value["location_y"]["Points"].append(Point(frame, cos(frame * 0.05) * 0.4, BEZIER).JsonValue());
        if (frame == existing_keys)
            timeline.ApplyJsonDiff(Json::writeString(writer, diff));
        if (frame > existing_keys)
            payloads.push_back(Json::writeString(writer, diff));
    }
    auto start = Clock::now();
    for (const auto& payload : payloads)
        timeline.ApplyJsonDiff(payload);
    double elapsed = chrono::duration<double>(Clock::now() - start).count();
    // A broken/no-op diff must not masquerade as an optimization.
    const int final_frame = existing_keys + edits;
    if (clip.location_x.GetCount() != final_frame || clip.location_y.GetCount() != final_frame ||
        abs(clip.location_x.GetValue(final_frame) - sin(final_frame * 0.05) * 0.4) > 1e-6 ||
        abs(clip.location_y.GetValue(final_frame) - cos(final_frame * 0.05) * 0.4) > 1e-6)
        throw runtime_error("Transform JSON benchmark did not apply the expected keyframes");
    timeline.Close();
    timeline.RemoveClip(&clip);
    return {edits, elapsed};
}

#if defined(OPENSHOT_HAS_AUDIOVISUALIZATION) || defined(OPENSHOT_HAS_BEATSYNC)
static std::shared_ptr<Frame> make_audio_visualization_frame(int64_t frame_number) {
    const int width = 1280;
    const int height = 720;
    const int sample_rate = 48000;
    const int samples = 1600;
    constexpr double pi = 3.14159265358979323846;

    auto frame = std::make_shared<Frame>(frame_number, width, height, "#00000000", samples, 2);
    auto image = std::make_shared<QImage>(width, height, QImage::Format_RGBA8888_Premultiplied);
    image->fill(Qt::transparent);
    frame->AddImage(image);
    frame->ResizeAudio(2, samples, sample_rate, LAYOUT_STEREO);

    std::vector<float> left(samples);
    std::vector<float> right(samples);
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i + frame_number * samples) / sample_rate;
        left[i] = static_cast<float>((std::sin(2.0 * pi * 110.0 * t) * 0.22) +
                                     (std::sin(2.0 * pi * 440.0 * t) * 0.48) +
                                     (std::sin(2.0 * pi * 1760.0 * t) * 0.18));
        right[i] = static_cast<float>((std::sin(2.0 * pi * 165.0 * t) * 0.18) +
                                      (std::sin(2.0 * pi * 660.0 * t) * 0.42) +
                                      (std::sin(2.0 * pi * 2640.0 * t) * 0.16));
    }
    frame->AddAudio(true, 0, 0, left.data(), samples, 1.0f);
    frame->AddAudio(true, 1, 0, right.data(), samples, 1.0f);
    return frame;
}
#endif

#ifdef OPENSHOT_HAS_AUDIOVISUALIZATION
static void run_audio_visualization_mode(int mode, int64_t start_frame, int64_t frames) {
    AudioVisualization effect;
    effect.visualization_type = mode;
    effect.background = AUDIO_VISUALIZATION_BACKGROUND_TRANSPARENT;
    effect.detail = Keyframe(0.75);
    effect.glow = Keyframe(0.25);
    effect.intensity = Keyframe(1.25);

    for (int64_t i = 0; i < frames; ++i) {
        int64_t fn = start_frame + i;
        effect.GetFrame(make_audio_visualization_frame(fn), fn);
    }
}

static TrialResult timed_audio_viz(int mode) {
    auto t0 = Clock::now();
    run_audio_visualization_mode(mode, START_FRAME, BENCH_FRAMES);
    return {BENCH_FRAMES, chrono::duration<double>(Clock::now() - t0).count()};
}
#endif // OPENSHOT_HAS_AUDIOVISUALIZATION

int main(int argc, char* argv[]) {
    // QApplication is required by any effect that renders text (e.g. Caption).
    // It must be alive for the duration of main.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);

    const string base = TEST_MEDIA_PATH;
    const string video = base + "sintel_trailer-720p.mp4";
    const string overlay = base + "front3.png";
    benchmark::BenchmarkOptions options;

    try {
        vector<string> args;
        args.reserve(std::max(0, argc - 1));
        for (int i = 1; i < argc; ++i)
            args.emplace_back(argv[i]);
        options = benchmark::ParseBenchmarkOptions(args);
    } catch (const std::exception& e) {
        cerr << e.what() << "\n";
        cerr << benchmark::BenchmarkUsage() << "\n";
        return 1;
    }

    if (options.show_help) {
        cout << benchmark::BenchmarkUsage() << "\n";
        return 0;
    }

    // Suppress FFmpeg/codec log output so it doesn't interleave with results.
    av_log_set_level(AV_LOG_QUIET);

    Settings *settings = Settings::Instance();
    if (options.omp_threads > 0) {
        settings->OMP_THREADS = options.omp_threads;
        settings->ApplyOpenMPSettings();
    }
    if (options.ff_threads > 0) {
        settings->FF_THREADS = options.ff_threads;
    }

    vector<Trial> trials;
    trials.reserve(40);

    for (int keys : {0, 1000}) {
        trials.emplace_back("Timeline JSON transforms (" + to_string(keys) + " existing keys)",
            [&, keys]() { return timed_transform_json(overlay, keys); });
    }


    trials.emplace_back("FFmpegReader", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        TrialResult result = timed_read(r);
        r.Close();
        return result;
    });

    trials.emplace_back("FFmpegWriter", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        FFmpegWriter w("benchmark_output.mp4");
        w.SetAudioOptions("aac", r.info.sample_rate, 192000);
        w.SetVideoOptions("libx264", r.info.width, r.info.height, r.info.fps, 5000000);
        w.Open();
        auto t0 = Clock::now();
        for (int64_t i = 0; i < BENCH_FRAMES; ++i)
            w.WriteFrame(r.GetFrame(START_FRAME + i));
        double elapsed = chrono::duration<double>(Clock::now() - t0).count();
        w.Close();
        r.Close();
        return {BENCH_FRAMES, elapsed};
    });

    trials.emplace_back("FrameMapper", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        FrameMapper map(&r, Fraction(30000, 1001), PULLDOWN_NONE, r.info.sample_rate,
                        r.info.channels, r.info.channel_layout);
        map.Open();
        TrialResult result = timed_read(map);
        map.Close();
        r.Close();
        return result;
    });

    trials.emplace_back("Clip", [&]() -> TrialResult {
        Clip c(video);
        c.Open();
        TrialResult result = timed_read(c);
        c.Close();
        return result;
    });

    trials.emplace_back("Timeline", [&]() -> TrialResult {
        Timeline t(1280, 720, Fraction(30000, 1001), 44100, 2, LAYOUT_STEREO);
        Clip video_clip(video);
        video_clip.Layer(0);
        video_clip.Start(0.0);
        video_clip.End(video_clip.Reader()->info.duration);
        video_clip.Open();
        Clip overlay1(overlay);
        overlay1.Layer(1);
        overlay1.Start(0.0);
        overlay1.End(video_clip.Reader()->info.duration);
        overlay1.Open();
        Clip overlay2(overlay);
        overlay2.Layer(2);
        overlay2.Start(0.0);
        overlay2.End(video_clip.Reader()->info.duration);
        overlay2.Open();
        t.AddClip(&video_clip);
        t.AddClip(&overlay1);
        t.AddClip(&overlay2);
        t.Open();
        t.info.video_length = t.GetMaxFrame();
        TrialResult result = timed_read(t);
        t.Close();
        return result;
    });

    trials.emplace_back("Timeline (with transforms)", [&]() -> TrialResult {
        Timeline t(1280, 720, Fraction(30000, 1001), 44100, 2, LAYOUT_STEREO);
        Clip video_clip(video);
        int64_t last = video_clip.Reader()->info.video_length;
        video_clip.Layer(0);
        video_clip.Start(0.0);
        video_clip.End(video_clip.Reader()->info.duration);
        video_clip.alpha.AddPoint(1, 1.0);
        video_clip.alpha.AddPoint(last, 0.0);
        video_clip.Open();
        Clip overlay1(overlay);
        overlay1.Layer(1);
        overlay1.Start(0.0);
        overlay1.End(video_clip.Reader()->info.duration);
        overlay1.Open();
        overlay1.scale_x.AddPoint(1, 1.0);
        overlay1.scale_x.AddPoint(last, 0.25);
        overlay1.scale_y.AddPoint(1, 1.0);
        overlay1.scale_y.AddPoint(last, 0.25);
        Clip overlay2(overlay);
        overlay2.Layer(2);
        overlay2.Start(0.0);
        overlay2.End(video_clip.Reader()->info.duration);
        overlay2.Open();
        overlay2.rotation.AddPoint(1, 90.0);
        overlay2.margin = Keyframe(0.03);
        overlay2.corner_radius = Keyframe(0.2);
        t.AddClip(&video_clip);
        t.AddClip(&overlay1);
        t.AddClip(&overlay2);
        t.Open();
        t.info.video_length = t.GetMaxFrame();
        TrialResult result = timed_read(t);
        t.Close();
        return result;
    });

#ifdef OPENSHOT_HAS_MASK
    const string mask_img = base + "mask.png";
    trials.emplace_back("Effect_Mask", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
#  ifdef USE_IMAGEMAGICK
        ImageReader mask_reader(mask_img);
#  else
        QtImageReader mask_reader(mask_img);
#  endif
        mask_reader.Open();
        Clip clip(&r);
        clip.Open();
        Mask m(&mask_reader, Keyframe(0.0), Keyframe(0.5));
        clip.AddEffect(&m);
        TrialResult result = timed_read(clip);
        mask_reader.Close();
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_BRIGHTNESS
    trials.emplace_back("Effect_Brightness", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Brightness b(Keyframe(0.5), Keyframe(1.0));
        clip.AddEffect(&b);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_CROP
    trials.emplace_back("Effect_Crop", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Crop c(Keyframe(0.25), Keyframe(0.25), Keyframe(0.25), Keyframe(0.25));
        clip.AddEffect(&c);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_SATURATION
    trials.emplace_back("Effect_Saturation", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Saturation s(Keyframe(0.25), Keyframe(0.25), Keyframe(0.25), Keyframe(0.25));
        clip.AddEffect(&s);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_CHROMAKEY
    trials.emplace_back("Effect_ChromaKey_BASIC", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        ChromaKey key(Color(0, 255, 0, 255), Keyframe(80.0), Keyframe(20.0), CHROMAKEY_BASIC);
        clip.AddEffect(&key);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });

    trials.emplace_back("Effect_ChromaKey_BASIC_SOFT", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        ChromaKey key(Color(0, 255, 0, 255), Keyframe(80.0), Keyframe(20.0), CHROMAKEY_BASIC_SOFT);
        clip.AddEffect(&key);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_COLORGRADE
    trials.emplace_back("Effect_ColorGrade", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        ColorGrade cg;
        clip.AddEffect(&cg);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_FILMGRAIN
    trials.emplace_back("Effect_FilmGrain", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        FilmGrain fg;
        clip.AddEffect(&fg);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_SHADOW
    trials.emplace_back("Effect_Shadow", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Shadow s;
        clip.AddEffect(&s);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_GLOW
    trials.emplace_back("Effect_Glow", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Glow g;
        clip.AddEffect(&g);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_BLUR
    trials.emplace_back("Effect_Blur", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        // Gaussian-style: radius=4 px, sigma=3.0, 3 iterations
        Blur b(Keyframe(4), Keyframe(4), Keyframe(3.0), Keyframe(3));
        clip.AddEffect(&b);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_SHARPEN
    trials.emplace_back("Effect_Sharpen", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Sharpen s(Keyframe(1.0), Keyframe(3.0), Keyframe(0.1));
        clip.AddEffect(&s);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_DENOISEIMAGE
    trials.emplace_back("Effect_DenoiseImage", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        DenoiseImage d;
        clip.AddEffect(&d);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_HUE
    trials.emplace_back("Effect_Hue", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Hue h(Keyframe(0.25));
        clip.AddEffect(&h);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_NEGATE
    trials.emplace_back("Effect_Negate", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Negate n;
        clip.AddEffect(&n);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_PIXELATE
    trials.emplace_back("Effect_Pixelate", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        // 5 % pixelization across the full frame
        Pixelate p(Keyframe(0.05), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0), Keyframe(0.0));
        clip.AddEffect(&p);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_WAVE
    trials.emplace_back("Effect_Wave", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Wave w(Keyframe(0.5), Keyframe(3.0), Keyframe(0.5), Keyframe(0.0), Keyframe(2.0));
        clip.AddEffect(&w);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_CAPTION
    {
        const char* qt_platform = std::getenv("QT_QPA_PLATFORM");
        const bool headless = qt_platform && std::string(qt_platform) == "offscreen";
        if (!headless)
    trials.emplace_back("Effect_Caption", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Caption c("WEBVTT\n\n"
                  "00:00:10.000 --> 00:00:15.000\nThis is a test caption.\n\n"
                  "00:00:15.000 --> 00:00:20.000\nSecond caption line.");
        clip.AddEffect(&c);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
    }  // headless guard
#endif

#ifdef OPENSHOT_HAS_TIMER
    {
        const char* qt_platform = std::getenv("QT_QPA_PLATFORM");
        const bool headless = qt_platform && std::string(qt_platform) == "offscreen";
        if (!headless)
    trials.emplace_back("Effect_Timer", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
        Clip clip(&r);
        clip.Open();
        Timer timer;
        timer.mode = TIMER_MODE_COUNT_UP;
        timer.format = TIMER_FORMAT_HH_MM_SS_MILLISECONDS;
        timer.prefix = "T ";
        clip.AddEffect(&timer);
        TrialResult result = timed_read(clip);
        clip.Close();
        r.Close();
        return result;
    });
    }  // headless guard
#endif

#ifdef OPENSHOT_HAS_DISPLACE
    trials.emplace_back("Effect_Displace", [&]() -> TrialResult {
        FFmpegReader r(video);
        r.Open();
#  ifdef USE_IMAGEMAGICK
        ImageReader map_reader(overlay);
#  else
        QtImageReader map_reader(overlay);
#  endif
        map_reader.Open();
        Clip clip(&r);
        clip.Open();
        Displace d(&map_reader, Keyframe(0.5), Keyframe(0.1), Keyframe(0.1), Keyframe(0.5), Keyframe(1.5));
        clip.AddEffect(&d);
        TrialResult result = timed_read(clip);
        map_reader.Close();
        clip.Close();
        r.Close();
        return result;
    });
#endif

#ifdef OPENSHOT_HAS_AUDIOVISUALIZATION
    const std::vector<std::pair<std::string, int>> audio_visualization_modes = {
        {"Effect_AudioVisualization_Waveform",       AUDIO_VISUALIZATION_WAVEFORM},
        {"Effect_AudioVisualization_FilledWaveform", AUDIO_VISUALIZATION_FILLED_WAVEFORM},
        {"Effect_AudioVisualization_Bars",           AUDIO_VISUALIZATION_BARS},
        {"Effect_AudioVisualization_Radial",         AUDIO_VISUALIZATION_RADIAL},
        {"Effect_AudioVisualization_Spectrum",       AUDIO_VISUALIZATION_SPECTRUM},
        {"Effect_AudioVisualization_PhaseScope",     AUDIO_VISUALIZATION_PHASE_SCOPE},
        {"Effect_AudioVisualization_Particles",      AUDIO_VISUALIZATION_PARTICLES},
        {"Effect_AudioVisualization_VUMeter",        AUDIO_VISUALIZATION_VU_METER},
        {"Effect_AudioVisualization_RadialBars",     AUDIO_VISUALIZATION_RADIAL_BARS}
    };

    for (const auto& mode : audio_visualization_modes) {
        trials.emplace_back(mode.first, [mode]() -> TrialResult {
            return timed_audio_viz(mode.second);
        });
    }

    trials.emplace_back("Effect_AudioVisualization_SpectrumModes", [&]() -> TrialResult {
        const std::vector<int> modes = {
            AUDIO_VISUALIZATION_BARS,
            AUDIO_VISUALIZATION_RADIAL,
            AUDIO_VISUALIZATION_SPECTRUM,
            AUDIO_VISUALIZATION_PARTICLES,
            AUDIO_VISUALIZATION_RADIAL_BARS
        };

        auto t0 = Clock::now();
        for (int mode : modes)
            run_audio_visualization_mode(mode, START_FRAME, BENCH_FRAMES);
        double elapsed = chrono::duration<double>(Clock::now() - t0).count();
        return {static_cast<int64_t>(modes.size()) * BENCH_FRAMES, elapsed};
    });
#endif

#ifdef OPENSHOT_HAS_BEATSYNC
    trials.emplace_back("Effect_BeatSync", [&]() -> TrialResult {
        BeatSync effect;
        effect.frequency_low  = Keyframe(0.0);   // full band
        effect.frequency_high = Keyframe(1.0);
        effect.threshold      = Keyframe(0.05);
        effect.attack_ms      = Keyframe(10.0);
        effect.decay_ms       = Keyframe(200.0);

        auto t0 = Clock::now();
        for (int64_t i = 0; i < BENCH_FRAMES; ++i) {
            int64_t fn = START_FRAME + i;
            effect.GetFrame(make_audio_visualization_frame(fn), fn);
        }
        return {BENCH_FRAMES, chrono::duration<double>(Clock::now() - t0).count()};
    });
#endif

#ifdef OPENSHOT_HAS_FRAMESCOPE
    trials.emplace_back("FrameScope", [&]() -> TrialResult {
        // Mirror the openshot-qt playback path from preview_thread.py:
        //   FrameScope() → SetWaveformColumns(widget_width) → SetVectorscopeSize(128)
        //   → SetFrame(frame) → individual typed getters (no JsonValue).
        // Vectorscope size 128 is the value used during live playback when
        // both waveform and vectorscope panels are visible.
        FFmpegReader r(video);
        r.Open();
        auto t0 = Clock::now();
        for (int64_t i = 0; i < BENCH_FRAMES; ++i) {
            auto frame = r.GetFrame(START_FRAME + i);
            FrameScope scope;
            scope.SetWaveformColumns(256);
            scope.SetVectorscopeSize(128);
            scope.SetFrame(frame);  // triggers analyze()
            // Read the same outputs openshot-qt reads
            scope.GetVideoWaveformLuma();
            scope.GetVideoHistogramLuma();
            scope.GetVideoVectorscope();
            scope.GetVideoAverageLuma();
            scope.GetVideoClippedShadows();
            scope.GetVideoClippedHighlights();
            scope.GetAudioChannels();
        }
        double elapsed = chrono::duration<double>(Clock::now() - t0).count();
        r.Close();
        return {BENCH_FRAMES, elapsed};
    });
#endif

    if (options.list_only) {
        for (const auto& trial : trials)
            cout << trial.first << "\n";
        return 0;
    }

    vector<BenchmarkRecord> records;
    records.reserve(trials.size());
    for (const auto& trial : trials) {
        if (!options.filter_test.empty() && trial.first != options.filter_test)
            continue;
        records.push_back(run_trial(trial.first, trial.second));
    }

    if (!options.filter_test.empty() && records.empty()) {
        cerr << "Unknown test: " << options.filter_test << "\nAvailable tests:\n";
        for (const auto& trial : trials)
            cerr << "  " << trial.first << "\n";
        return 2;
    }

    if (isatty(STDOUT_FILENO))
        cout << "\r" << string(64, ' ') << "\r" << flush;
    print_results(records);
    return 0;
}
