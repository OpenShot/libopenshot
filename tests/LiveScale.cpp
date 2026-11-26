/**
 * @file
 * @brief Live scaling must preserve source detail and inexpensive transform edits.
 * @author Jonathan Thomas <jonathan@openshot.org>
 */
// Copyright (c) 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "Clip.h"
#include "DummyReader.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "GpuMetrics.h"
#include "QtImageReader.h"
#include "QtTextReader.h"
#include "Timeline.h"
#ifdef OPENSHOT_ENABLE_GPU_FRAME
#include "VulkanFrameImage.h"
#include "GpuDevices.h"
extern "C" {
#include <libavutil/buffer.h>
}
#endif
#include <QFile>
#include <QGuiApplication>
#include <QFont>
#include <QTemporaryDir>
#include <cmath>
#include <functional>
#include <vector>

using namespace openshot;

namespace {
int app_argc = 1;
char app_name[] = "live-scale";
char* app_argv[] = {app_name, nullptr};
int& font_args() {
#ifdef _WIN32
    if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR")) {
        const auto root = qgetenv("SystemRoot");
        qputenv("QT_QPA_FONTDIR", (root.isEmpty() ? QByteArray("C:\\Windows") : root) + "\\Fonts");
    }
#endif
    return app_argc;
}
struct Application : QGuiApplication {
    Application() : QGuiApplication(font_args(), app_argv) {}
};

bool backend_available() {
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    if (qEnvironmentVariableIsSet("OPENSHOT_TEST_REQUIRE_VULKAN")) {
        AVBufferRef* device = nullptr;
        if (!VulkanRenderingEnabled() || CreateSharedVulkanDevice(&device) < 0) {
            WARN("Skipping Vulkan test: no Vulkan device is available");
            return false;
        }
        av_buffer_unref(&device);
    }
#endif
    return true;
}

void edit(Timeline& timeline, const Clip& clip, const Json::Value& values) {
    Json::Value change, changes(Json::arrayValue), id;
    change["type"] = "update";
    change["key"].append("clips");
    id["id"] = clip.Id();
    change["key"].append(id);
    change["value"] = values;
    changes.append(change);
    timeline.ApplyJsonDiff(changes.toStyledString());
}

Json::Value scaled(double value, bool incremental = false) {
    Json::Value result;
    if (incremental) {
        result["scale_x"]["Point"]["co"]["X"] = 1;
        result["scale_x"]["Point"]["co"]["Y"] = value;
    } else {
        result["scale_x"] = Keyframe(value).JsonValue();
    }
    result["scale_y"] = result["scale_x"];
    return result;
}

QImage pixels(Timeline& timeline) {
    auto frame = timeline.GetFrame(1);
    REQUIRE(frame);
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    if (qEnvironmentVariableIsSet("OPENSHOT_TEST_REQUIRE_VULKAN")) {
        REQUIRE(VulkanRenderingEnabled());
        REQUIRE(std::dynamic_pointer_cast<const VulkanFrameImage>(frame->GetDeferredImage()));
    }
#endif
    // Read back a copy: GetImage() exposes mutable pixels and retires a GPU
    // surface. Inspecting a test result must not alter the cached frame.
    Frame copy(*frame);
    return copy.GetImage()->convertToFormat(QImage::Format_RGBA8888);
}

Json::UInt64 metric(const char* name) {
    Json::Value metrics;
    Json::Reader reader;
    REQUIRE(reader.parse(GpuMetricsSnapshot(), metrics));
    return metrics[name].asUInt64();
}

Json::UInt64 uploads() { return metric("uploads"); }

using Source = std::function<std::unique_ptr<ReaderBase>()>;
QImage fresh_pixels(const Clip& edited, const Source& source) {
    auto reader = source();
    reader->Open();
    Clip clip;
    clip.Reader(reader.get());
    auto data = edited.JsonValue();
    data.removeMember("reader");
    clip.SetJsonValue(data);
    Timeline timeline(192, 192, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.SetMaxSize(192, 192);
    timeline.Open();
    auto result = pixels(timeline);
    timeline.Close();
    return result;
}

double soft_fraction(const QImage& image) {
    int ink = 0, soft = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const int value = image.pixelColor(x, y).red();
            if (value > 16) { ++ink; soft += value < 240; }
        }
    REQUIRE(ink > 100);
    return soft / double(ink);
}

// A decoded video frame with a deterministic sharp pattern and real audio.
// Counting source reads detects decode/remapping regressions without timing thresholds.
class PatternReader : public DummyReader {
public:
    int reads = 0;
    PatternReader() : DummyReader(Fraction(30, 1), 384, 384, 48000, 2, 10) {
        info.has_audio = true;
    }
    std::shared_ptr<Frame> GetFrame(int64_t number) override {
        ++reads;
        auto frame = std::make_shared<Frame>(number, 384, 384, "black", 1600, 2);
        auto image = std::make_shared<QImage>(384, 384, QImage::Format_RGBA8888_Premultiplied);
        for (int y = 0; y < 384; ++y)
            for (int x = 0; x < 384; ++x)
                image->setPixelColor(x, y, (x / 12 + y / 12) % 2 ? Qt::white : Qt::black);
        frame->AddImage(image);
        frame->ResizeAudio(2, 1600, 48000, LAYOUT_STEREO);
        std::vector<float> audio(1600);
        for (int i = 0; i < 1600; ++i)
            audio[i] = 0.5f * std::sin(i * 0.03);
        for (int channel = 0; channel < 2; ++channel)
            frame->AddAudio(true, channel, 0, audio.data(), audio.size(), 1.0f);
        return frame;
    }
};
}

TEST_CASE("Live artwork scaling matches fresh detail without discarding reusable rasters", "[libopenshot][live-scale]") {
    Application application;
    if (!backend_available()) return;
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const int kind = GENERATE(0, 1, 2);
    const bool svg = kind != 2;
    INFO("Artwork: " << kind << " (triangle SVG, emoji SVG, PNG)");
    const auto path = directory.filePath(svg ? "emoji.svg" : "artwork.png");
    if (kind == 1) {
        REQUIRE(QFile::copy(QString(TEST_MEDIA_PATH) + "1F0CF.svg", path));
    } else if (svg) {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        // A small diagonal motif leaves room to enlarge inside the canvas.
        file.write("<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64'>"
                   "<path fill='white' d='M24 40 L32 24 L40 40 Z'/></svg>");
    } else {
        QImage image(768, 768, QImage::Format_RGBA8888_Premultiplied);
        for (int y = 0; y < 768; ++y)
            for (int x = 0; x < 768; ++x)
                image.setPixelColor(x, y, (x / 12 + y / 12) % 2 ? Qt::white : Qt::black);
        REQUIRE(image.save(path));
    }
    // Inspect each independent reader once. The inspecting reader and Clip
    // constructors would otherwise open/close the same SVG repeatedly, loading
    // system fonts every time, before the timeline even renders a frame.
    Source source = [path] { return std::unique_ptr<ReaderBase>(new QtImageReader(path.toStdString(), false)); };
    auto reader = source();
    reader->Open();
    Clip clip;
    clip.Reader(reader.get());
    clip.End(reader->info.duration);
    clip.Id("artwork");
    clip.scale = GENERATE(SCALE_FIT, SCALE_NONE);
    Timeline timeline(192, 192, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.SetMaxSize(192, 192);
    timeline.Open();
    auto* mapper = dynamic_cast<FrameMapper*>(clip.Reader());
    REQUIRE(mapper);
    const auto initial = pixels(timeline);
    for (double scale : {2.0, 3.0, 1.0, 2.0}) {
        INFO("Scale: " << scale);
        const auto dragging = mapper->GetFrame(1);
        timeline.BeginClipTransform(clip.Id());
        edit(timeline, clip, scaled(scale, scale == 3.0));
        pixels(timeline);
        CHECK(mapper->GetFrame(1) == dragging);
        CHECK(timeline.EndClipTransform(clip.Id()));
        const auto live = pixels(timeline);
        const int base_width = clip.scale == SCALE_NONE ? reader->info.width : 192;
        CHECK(mapper->GetFrame(1)->GetWidth() == int(base_width * scale));
        CHECK(live == fresh_pixels(clip, source));
        if (kind == 0 && scale == 3.0) {
            CHECK(soft_fraction(live) < soft_fraction(initial) * 0.65);
            const auto bounds = [](const QImage& image) {
                QRect result;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x)
                        if (image.pixelColor(x, y).red() > 16) result |= QRect(x, y, 1, 1);
                return result;
            };
            CHECK(bounds(live).width() == Approx(bounds(initial).width() * 3).margin(4));
            CHECK(bounds(live).height() == Approx(bounds(initial).height() * 3).margin(4));
        }
    }
    // Move, rotate and fade must reuse the exact mapped source frame.
    const auto cached = mapper->GetFrame(1);
    Json::Value moved;
    moved["location_x"] = Keyframe(0.1).JsonValue();
    moved["rotation"] = Keyframe(12).JsonValue();
    moved["alpha"] = Keyframe(0.75).JsonValue();
    const auto uploaded = uploads();
    timeline.BeginClipTransform(clip.Id());
    edit(timeline, clip, moved);
    CHECK_FALSE(timeline.EndClipTransform(clip.Id()));
    const auto moved_pixels = pixels(timeline);
    CHECK(uploads() == uploaded);
    CHECK(moved_pixels == fresh_pixels(clip, source));
    CHECK(mapper->GetFrame(1) == cached);

    // An existing animation maximum already provides enough source pixels.
    Keyframe animated(1.0);
    animated.AddPoint(100, 3.0, LINEAR);
    Json::Value curves;
    curves["scale_x"] = curves["scale_y"] = animated.JsonValue();
    edit(timeline, clip, curves);
    pixels(timeline);
    const auto detailed = mapper->GetFrame(1);
    const auto detailed_uploads = uploads();
    timeline.BeginClipTransform(clip.Id());
    edit(timeline, clip, scaled(2.0, true));
    CHECK_FALSE(timeline.EndClipTransform(clip.Id()));
    const auto updated = pixels(timeline);
    CHECK(uploads() == detailed_uploads);
    CHECK(updated == fresh_pixels(clip, source));
    CHECK(mapper->GetFrame(1) == detailed);
    timeline.Close();
}

TEST_CASE("Live video and waveform transforms preserve decoded frames and fresh output", "[libopenshot][live-scale]") {
    Application application;
    if (!backend_available()) return;
    const bool waveform = GENERATE(false, true);
    INFO("Waveform: " << waveform);
    PatternReader reader;
    Clip clip(&reader);
    clip.Id("media");
    clip.Waveform(waveform);
    Timeline timeline(192, 192, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.SetMaxSize(192, 192);
    timeline.Open();
    auto* mapper = dynamic_cast<FrameMapper*>(clip.Reader());
    REQUIRE(mapper);
    const auto initial = pixels(timeline);
    const auto cached = mapper->GetFrame(1);
    const int reads = reader.reads;
    Source source = [] { return std::unique_ptr<ReaderBase>(new PatternReader()); };
    for (double scale : {2.0, 3.0, 1.0}) {
        const auto uploaded = uploads();
        const auto readbacks = metric("readbacks");
        timeline.BeginClipTransform(clip.Id());
        edit(timeline, clip, scaled(scale));
        pixels(timeline);
        CHECK(uploads() == uploaded);
        CHECK(mapper->GetFrame(1) == cached);
        CHECK(timeline.EndClipTransform(clip.Id()) == waveform);
        const auto live = pixels(timeline);
        CHECK(metric("readbacks") == readbacks);
        if (!waveform) CHECK(uploads() == uploaded);
        CHECK(live == fresh_pixels(clip, source));
        CHECK(mapper->GetFrame(1) == cached);
        CHECK(reader.reads == reads);
        if (scale == 2.0) {
            CHECK(live != initial);
            // At 2x the 384px decoded pattern maps 1:1 into the 192px crop.
            if (!waveform) CHECK(soft_fraction(live) < 0.02);
        }
    }
    timeline.Close();
}

TEST_CASE("Text scale edits reuse detail already supplied for animation", "[libopenshot][live-scale]") {
    Application application;
    if (!backend_available()) return;
    Source source = [] {
        auto reader = std::unique_ptr<QtTextReader>(new QtTextReader());
        auto data = reader->JsonValue();
        data["width"] = 96;
        data["height"] = 48;
        data["text"] = "Hi";
        data["document_layout"] = true;
        data["background_color"] = "transparent";
        QFont font("DejaVu Sans");
        font.setPixelSize(24);
        data["font"] = font.toString().toStdString();
        reader->SetJsonValue(data);
        return std::unique_ptr<ReaderBase>(reader.release());
    };
    auto reader = source();
    Clip clip(reader.get());
    clip.Id("text");
    clip.scale = SCALE_NONE;
    clip.scale_x.AddPoint(100, 3.0, LINEAR);
    clip.scale_y = clip.scale_x;
    Timeline timeline(192, 192, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.SetMaxSize(192, 192);
    timeline.Open();
    pixels(timeline);
    auto* mapper = dynamic_cast<FrameMapper*>(clip.Reader());
    REQUIRE(mapper);
    const auto cached = mapper->GetFrame(1);
    CHECK(cached->GetWidth() == 288);
    const auto uploaded = uploads();
    timeline.BeginClipTransform(clip.Id());
    edit(timeline, clip, scaled(2.0, true));
    CHECK_FALSE(timeline.EndClipTransform(clip.Id()));
    const auto updated = pixels(timeline);
    CHECK(uploads() == uploaded);
    CHECK(updated == fresh_pixels(clip, source));
    CHECK(mapper->GetFrame(1) == cached);
    // A point beyond the old maximum must refresh the source.
    timeline.BeginClipTransform(clip.Id());
    edit(timeline, clip, scaled(4.0, true));
    pixels(timeline);
    CHECK(mapper->GetFrame(1) == cached);
    CHECK(timeline.EndClipTransform(clip.Id()));
    CHECK(pixels(timeline) == fresh_pixels(clip, source));
    CHECK(mapper->GetFrame(1) != cached);
    CHECK(mapper->GetFrame(1)->GetWidth() == 384);
    timeline.Close();
}

TEST_CASE("Real video dragging keeps preview pixels and refines once on release", "[libopenshot][live-scale]") {
    Application application;
    if (!backend_available()) return;
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path = directory.filePath("pattern.avi").toStdString();
    PatternReader pattern;
    FFmpegWriter writer(path);
    writer.SetVideoOptions(true, "rawvideo", Fraction(30, 1), 384, 384, Fraction(1, 1), false, false, 1000000);
    writer.Open();
    for (int i = 1; i <= 6; ++i) writer.WriteFrame(pattern.GetFrame(i));
    writer.Close();
    FFmpegReader reader(path);
    Clip clip(&reader);
    clip.Id("video");
    Timeline timeline(192, 192, Fraction(30, 1), 48000, 2, LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.SetMaxSize(192, 192);
    timeline.Open();
    pixels(timeline);
    auto* mapper = dynamic_cast<FrameMapper*>(clip.Reader());
    REQUIRE(mapper);
    const auto preview = mapper->GetFrame(1);
    CHECK(preview->GetWidth() == 192);
    timeline.BeginClipTransform(clip.Id());
    for (double scale : {1.25, 1.5, 2.0}) {
        edit(timeline, clip, scaled(scale));
        pixels(timeline);
        CHECK(mapper->GetFrame(1) == preview);
    }
    CHECK(timeline.EndClipTransform(clip.Id()));
    const auto refined = pixels(timeline);
    CHECK(mapper->GetFrame(1)->GetWidth() == 384);
    Source source = [path] { return std::unique_ptr<ReaderBase>(new FFmpegReader(path)); };
    CHECK(refined == fresh_pixels(clip, source));
    const auto native = mapper->GetFrame(1);
    // Scaling beyond the available source pixels must not repeatedly decode.
    timeline.BeginClipTransform(clip.Id());
    edit(timeline, clip, scaled(3.0));
    CHECK_FALSE(timeline.EndClipTransform(clip.Id()));
    CHECK(pixels(timeline) == fresh_pixels(clip, source));
    CHECK(mapper->GetFrame(1) == native);
    CHECK_FALSE(timeline.EndClipTransform(clip.Id()));
    timeline.Close();
}
