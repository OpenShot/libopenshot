// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifdef __APPLE__
#include "MacVulkanInterop.h"
#include <CoreVideo/CoreVideo.h>
#endif
#define CATCH_CONFIG_EXTERNAL_INTERFACES
#include "openshot_catch.h"
#include "VulkanCompositor.h"
#include "VulkanFrameImage.h"
#include "VulkanEncodeConverter.h"
#include "ColorManagement.h"
#include "Settings.h"
#include <array>
#include "GpuDevices.h"
#include "GpuMetrics.h"
#include "FrameScope.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include <QTemporaryDir>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPathStroker>
#include <algorithm>
#include <numeric>
#include "Clip.h"
#include "effects/Saturation.h"
#include "effects/Crop.h"
#include "effects/ColorGrade.h"
#include "effects/ColorMap.h"
#include <QFile>
#include <QTextStream>
#include <cmath>
#include "effects/Mask.h"
#include "effects/Blur.h"
#include "effects/Distort.h"
#include "effects/Brightness.h"
#include "effects/Hue.h"
#include "effects/Negate.h"
#include "effects/Bars.h"
#include "effects/Shift.h"
#include "effects/ColorShift.h"
#include "effects/ChromaKey.h"
#include "effects/Sharpen.h"
#include "effects/Pixelate.h"
#include "effects/Shadow.h"
#include "effects/Glow.h"
#include "effects/Wave.h"
#include "effects/Deinterlace.h"
#include "effects/FilmGrain.h"
#include "effects/LensFlare.h"
#include "effects/SphericalProjection.h"
#ifdef USE_OPENCV
#include "effects/Stabilizer.h"
#include "effects/Tracker.h"
#include "effects/ObjectDetection.h"
#endif
#ifdef USE_OPENCV
#include "effects/ObjectMask.h"
#endif
#include "effects/DenoiseImage.h"
#include "effects/Displace.h"
#include "effects/AnalogTape.h"

#ifdef USE_OPENCV
#include "effects/Outline.h"
#endif
#include "QtImageReader.h"
#include "Timeline.h"
#include "DummyReader.h"
#include <map>
#include <cstdlib>
#include <memory>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libswscale/swscale.h>
}
namespace {
struct BufferDelete { void operator()(AVBufferRef* p) const { av_buffer_unref(&p); } };
struct FrameDelete { void operator()(AVFrame* p) const { av_frame_free(&p); } };
using Buffer=std::unique_ptr<AVBufferRef,BufferDelete>;
using Frame=std::unique_ptr<AVFrame,FrameDelete>;
// CTest reports this message as skipped; see tests/CMakeLists.txt.
bool VulkanAvailable() {
    if(!openshot::VulkanRenderingEnabled()) {
        WARN("Skipping Vulkan test: CPU mode, or no Vulkan library");
        return false;
    }
    AVBufferRef* device=nullptr;
    if(openshot::CreateSharedVulkanDevice(&device)<0) {
        WARN("Skipping Vulkan test: no Vulkan device is available");
        return false;
    }
    av_buffer_unref(&device);
    return true;
}
class PatternReader : public openshot::DummyReader {
    std::shared_ptr<const openshot::FrameImage> surface;
    QImage pixels;
public:
    PatternReader(std::shared_ptr<const openshot::FrameImage> image, bool gpu)
        : DummyReader(openshot::Fraction(24,1),image->GetWidth(),image->GetHeight(),48000,2,5.0),
          surface(gpu?image:nullptr),pixels(gpu?QImage():image->Materialize()) {}
    std::shared_ptr<openshot::Frame> GetFrame(int64_t n) override {
        auto frame=std::make_shared<openshot::Frame>(n,info.width,info.height,"#00000000");
        if(surface) frame->AddDeferredImage(surface);
        else frame->AddImage(std::make_shared<QImage>(pixels));
        return frame;
    }
};
}
namespace {
std::unique_ptr<QGuiApplication>& VulkanTestApplication() {
    static std::unique_ptr<QGuiApplication> app;
    return app;
}
#if CATCH_VERSION_MAJOR >= 3
using VulkanTestListenerBase=Catch::EventListenerBase;
#else
using VulkanTestListenerBase=Catch::TestEventListenerBase;
#endif
struct VulkanTestApplicationLifetime : VulkanTestListenerBase {
    using VulkanTestListenerBase::VulkanTestListenerBase;
    void testRunEnded(const Catch::TestRunStats&) override {
        // Destroy Qt before process-wide Qt/plugin statics are torn down.
        VulkanTestApplication().reset();
    }
};
CATCH_REGISTER_LISTENER(VulkanTestApplicationLifetime)
void EnsureVulkanTestApplication() {
    static int argc=1;static char name[]="vulkan-effects";static char* argv[]={name,nullptr};
    if(!QGuiApplication::instance()) VulkanTestApplication()=std::make_unique<QGuiApplication>(argc,argv);
}
}
TEST_CASE("Vulkan GPU listing reports the card OpenShot renders with", "[vulkan][hardware]") {
    if(!VulkanAvailable()) return;
    const auto active=openshot::ActiveGpuDevice();
    REQUIRE(!active.name.empty());
    CHECK(active.active);
    CHECK(active.type!="software");
    CHECK(active.uuid.size()==32);
    int active_count=0;
    for(const auto& gpu : openshot::ListGpuDevices()) {
        CHECK(!gpu.name.empty());
        if(gpu.active) { ++active_count; CHECK(gpu.uuid==active.uuid); }
    }
    CHECK(active_count==1);
    // Every reported encoder exists in this FFmpeg build; the list is cached.
    const auto encoders=openshot::GpuEncoders();
    for(const auto& name : encoders) CHECK(avcodec_find_encoder_by_name(name.c_str()));
    CHECK(openshot::GpuEncoders()==encoders);
}
TEST_CASE("AV1 prefers Vulkan decoding over the default software decoder", "[vulkan][hardware][reader]") {
    if (!VulkanAvailable()) return;
    if (!openshot::VulkanAv1DecodeSupported()) {
        WARN("Skipping Vulkan test: no Vulkan AV1 decode extension");
        return;
    }
    openshot::FFmpegReader reader(TEST_MEDIA_PATH "test_video_sync.mp4");
    reader.Open();
    auto frame = reader.GetFrame(90);
    REQUIRE(frame->GetDeferredImage());
    CHECK(reader.HardwareDecodeSuccessful());
    auto image = frame->GetImage();
    REQUIRE(image);
    CHECK(image->pixelColor(930, 820).red() > 250);
    reader.Close();
}

TEST_CASE("VA-API AV1 export accepts a requested bitrate", "[vulkan][hardware][writer]") {
    if(!VulkanAvailable()) return;
    const auto encoders=openshot::GpuEncoders();
    if(std::find(encoders.begin(),encoders.end(),"av1_vaapi")==encoders.end()){
        WARN("Skipping Vulkan test: no VA-API AV1 encoder");
        return;
    }
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path=directory.filePath("av1.mp4").toStdString();
    openshot::FFmpegWriter writer(path);
    writer.SetVideoOptions(true,"av1_vaapi",openshot::Fraction(24,1),256,256,openshot::Fraction(1,1),false,false,1000000);
    REQUIRE_NOTHROW(writer.Open());
    for(int n=1;n<=3;++n) writer.WriteFrame(std::make_shared<openshot::Frame>(n,256,256,"red"));
    writer.Close();
    openshot::FFmpegReader reader(path);
    reader.Open();
    const auto* decoder=avcodec_find_decoder_by_name(reader.info.vcodec.c_str());
    REQUIRE(decoder);
    CHECK(decoder->id==AV_CODEC_ID_AV1);
    auto image=reader.GetFrame(1)->GetImage();
    REQUIRE(image);
    if (openshot::VulkanAv1DecodeSupported())
        CHECK(reader.HardwareDecodeSuccessful());
    CHECK(image->pixelColor(128,128).red()>200);
    CHECK(image->pixelColor(128,128).blue()<30);
    reader.Close();
}
TEST_CASE("Native Vulkan export keeps decoded and composited frames on the GPU", "[vulkan][hardware][writer]") {
    if(!VulkanAvailable()) return;
    const auto encoders=openshot::GpuEncoders();
    if(std::find(encoders.begin(),encoders.end(),"h264_vulkan")==encoders.end()){
        WARN("Skipping Vulkan test: no native Vulkan H.264 encoder");
        return;
    }
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    // Repeat exports to exercise encoder surface reuse and owner cleanup.
    for(int pass=0;pass<2;++pass){
        const int width=pass?640:256,height=pass?360:256;
        openshot::FFmpegReader base_reader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
        openshot::FFmpegReader overlay_reader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
        openshot::Clip base(&base_reader),overlay(&overlay_reader);
        openshot::Timeline timeline(width,height,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
        timeline.SetMaxSize(width,height);
        base.Layer(0);overlay.Layer(1);
        overlay.scale_x=openshot::Keyframe(0.7);overlay.rotation=openshot::Keyframe(12.0);
        overlay.alpha=openshot::Keyframe(0.6);
        timeline.AddClip(&base);timeline.AddClip(&overlay);timeline.Open();
        const auto path=directory.filePath(QString("native-%1.mp4").arg(pass)).toStdString();
        openshot::FFmpegWriter writer(path);
        writer.SetVideoOptions(true,"h264_vulkan",openshot::Fraction(24,1),width,height,openshot::Fraction(1,1),false,false,1000000);
        writer.Open();
        REQUIRE(writer.info.vcodec=="h264_vulkan"); // CPU fallback must not pass this test.
        std::shared_ptr<openshot::Frame> expected_frame;
        const auto readbacks=openshot::VulkanFrameReadbackCount();
        for(int n=1;n<=24;++n){
            auto frame=timeline.GetFrame(n);
            REQUIRE(frame->GetDeferredImage());
            writer.WriteFrame(frame);
            REQUIRE(frame->GetDeferredImage());
            if (n == 12) expected_frame = frame;
        }
        writer.Close();
        CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
        CHECK(base_reader.HardwareDecodeSuccessful());
        CHECK(overlay_reader.HardwareDecodeSuccessful());
        timeline.Close();
        // Decode the finished bitstream on the CPU only after measuring export.
        openshot::FFmpegReader reader(path);
        reader.Open();
        CHECK(reader.info.width==width);CHECK(reader.info.height==height);
        CHECK(reader.info.video_length==24);
        auto image=reader.GetFrame(12)->GetImage();
        REQUIRE(image);CHECK(image->width()==width);CHECK(image->height()==height);
        reader.Close();
    }
}
TEST_CASE("NVENC export accepts CPU and Vulkan frames without changing the requested encoder", "[vulkan][hardware][writer][nvenc]") {
    if (!VulkanAvailable()) return;
    const auto encoders = openshot::GpuEncoders();
    if (std::find(encoders.begin(), encoders.end(), "h264_nvenc") == encoders.end()) {
        WARN("Skipping Vulkan test: no usable NVENC encoder");
        return;
    }
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    QImage pixels(256, 256, QImage::Format_RGBA8888_Premultiplied);
    for (int y = 0; y < pixels.height(); ++y)
        for (int x = 0; x < pixels.width(); ++x)
            pixels.setPixelColor(x, y, y < 128 ? Qt::red : Qt::blue);
    const auto uploaded = openshot::UploadVulkanImage(pixels);
    REQUIRE(uploaded);
    for (const std::string codec : {"h264_nvenc", "hevc_nvenc"}) {
        if (std::find(encoders.begin(), encoders.end(), codec) == encoders.end()) continue;
        for (bool gpu : {false, true}) {
            INFO("encoder=" << codec << " GPU input=" << gpu);
            const auto path = directory.filePath(QString::fromStdString(codec + (gpu ? "-gpu.mp4" : "-cpu.mp4"))).toStdString();
            openshot::FFmpegWriter writer(path);
            writer.SetVideoOptions(true, codec, openshot::Fraction(24, 1), 256, 256,
                                   openshot::Fraction(1, 1), false, false, 2000000);
            writer.Open();
            REQUIRE(writer.info.vcodec == codec);
            const auto before = openshot::VulkanFrameReadbackCount();
            for (int n = 1; n <= 8; ++n) {
                auto frame = std::make_shared<openshot::Frame>(n, 256, 256, "black");
                if (gpu) frame->AddDeferredImage(uploaded);
                else frame->AddImage(std::make_shared<QImage>(pixels));
                writer.WriteFrame(frame);
            }
            writer.Close();
            if (gpu) CHECK(openshot::VulkanFrameReadbackCount() == before);
            openshot::FFmpegReader reader(path);
            reader.Open();
            CHECK(reader.info.video_length == 8);
            const auto image = reader.GetFrame(4)->GetImage();
            REQUIRE(image);
            CHECK(image->pixelColor(128, 64).red() > 220);
            CHECK(image->pixelColor(128, 64).blue() < 25);
            CHECK(image->pixelColor(128, 192).blue() > 220);
            CHECK(image->pixelColor(128, 192).red() < 25);
            reader.Close();
        }
    }
}
TEST_CASE("Rounded webcam overlays remain GPU resident", "[vulkan][hardware][capture][rounded]") {
    if (!VulkanAvailable()) return;
    QImage pixels(64, 64, QImage::Format_RGBA8888_Premultiplied);
    pixels.fill(Qt::red);
    auto surface = openshot::UploadVulkanImage(pixels);
    PatternReader reader(surface, true);
    openshot::Clip clip(&reader);
    clip.corner_radius = openshot::Keyframe(0.25);
    clip.scale_x = openshot::Keyframe(0.75);
    clip.scale_y = openshot::Keyframe(0.75);
    openshot::Timeline timeline(64, 64, openshot::Fraction(24, 1), 48000, 2, openshot::LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    openshot::ResetGpuMetrics();
    auto frame = timeline.GetFrame(1);
    REQUIRE(frame->GetDeferredImage());
    Json::Reader parser;
    Json::Value metrics;
    REQUIRE(parser.parse(openshot::GpuMetricsSnapshot(), metrics));
    CHECK(metrics["readbacks"].asUInt64() == 0);
    CHECK(metrics["render_cpu"].asUInt64() == 0);
    auto image = frame->GetImage();
    CHECK(image->pixelColor(32, 32).red() > 240);
    CHECK(image->pixelColor(9, 9).red() < 15);
    timeline.Close();
    // Also exercise layer compositing over an existing GPU background.
    pixels.fill(Qt::blue);
    auto background = openshot::UploadVulkanImage(pixels);
    auto rounded = openshot::RenderVulkanImage(surface, background, 64, 64,
        QTransform(), 1.0, Qt::transparent, false, 0.25);
    auto result = rounded->Materialize();
    CHECK(result.pixelColor(32, 32) == QColor(Qt::red));
    CHECK(result.pixelColor(0, 0) == QColor(Qt::blue));
}
TEST_CASE("Vulkan capture encoding uploads CPU frames without readback", "[vulkan][hardware][writer][capture]") {
    if (!VulkanAvailable()) return;
    const auto encoders = openshot::GpuEncoders();
    if (std::find(encoders.begin(), encoders.end(), "h264_vulkan") == encoders.end()) {
        WARN("Skipping Vulkan test: no native Vulkan H.264 encoder");
        return;
    }
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    openshot::FFmpegWriter writer(directory.filePath("capture.mp4").toStdString());
    writer.SetVideoOptions(true, "h264_vulkan", openshot::Fraction(24, 1), 256, 256,
                           openshot::Fraction(1, 1), false, false, 1000000);
    writer.Open();
    REQUIRE(writer.info.vcodec == "h264_vulkan");
    const auto readbacks = openshot::VulkanFrameReadbackCount();
    openshot::ResetGpuMetrics(false);
    for (int n = 1; n <= 3; ++n) {
        auto frame = std::make_shared<openshot::Frame>(n, 256, 256, "red");
        REQUIRE_FALSE(frame->GetDeferredImage());
        writer.WriteFrameAt(frame, n);
        CHECK(frame->GetImage()->pixelColor(128, 128) == QColor(Qt::red));
    }
    writer.Close();
    CHECK(openshot::VulkanFrameReadbackCount() == readbacks);
    Json::Reader parser;
    Json::Value metrics;
    REQUIRE(parser.parse(openshot::GpuMetricsSnapshot(false), metrics));
    CHECK(metrics["uploads"].asUInt64() == 3);
    CHECK(metrics["encode_gpu"].asUInt64() == 3);
    CHECK(metrics["encode_cpu"].asUInt64() == 0);
    openshot::FFmpegReader reader(directory.filePath("capture.mp4").toStdString());
    reader.Open();
    const auto pixel = reader.GetFrame(2)->GetImage()->pixelColor(128, 128);
    CHECK(pixel.red() > 230);
    CHECK(pixel.green() < 25);
    CHECK(pixel.blue() < 25);
    reader.Close();
}
TEST_CASE("Vulkan export accepts CPU frames and keeps writers independent", "[vulkan][writer][fallback]") {
    // Run this with OPENSHOT_RENDER_BACKEND=cpu, an invalid Vulkan device, or
    // an older FFmpeg too: the requested codec must fall back before the header.
    if(!avcodec_find_encoder_by_name("libx264")){
        WARN("Skipping Vulkan test: CPU fallback test requires libx264");
        return;
    }
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto gpu_path=directory.filePath("gpu-or-fallback.mp4").toStdString();
    const auto cpu_path=directory.filePath("cpu.mp4").toStdString();
    openshot::FFmpegWriter gpu(gpu_path),cpu(cpu_path);
    gpu.SetVideoOptions(true,"h264_vulkan",openshot::Fraction(24,1),256,256,openshot::Fraction(1,1),false,false,1000000);
    cpu.SetVideoOptions(true,"libx264",openshot::Fraction(24,1),256,256,openshot::Fraction(1,1),false,false,1000000);
    gpu.Open();cpu.Open();
    if(!openshot::VulkanRenderingEnabled()) CHECK(gpu.info.vcodec!="h264_vulkan");
    for(int n=1;n<=3;++n){
        gpu.WriteFrame(std::make_shared<openshot::Frame>(n,256,256,"red"));
        cpu.WriteFrame(std::make_shared<openshot::Frame>(n,256,256,"blue"));
    }
    gpu.Close();cpu.Close();
    for(const auto& item:std::vector<std::pair<std::string,QColor>>{{gpu_path,Qt::red},{cpu_path,Qt::blue}}){
        openshot::FFmpegReader reader(item.first);
        reader.Open();
        CHECK(reader.info.video_length==3);
        auto image=reader.GetFrame(2)->GetImage();
        REQUIRE(image);
        const auto actual=image->pixelColor(128,128);
        CHECK(std::abs(actual.red()-item.second.red())<25);
        CHECK(std::abs(actual.blue()-item.second.blue())<25);
        reader.Close();
    }
}
TEST_CASE("Vulkan export recovers from an incompatible GPU surface", "[vulkan][hardware][fallback]") {
    if(!VulkanAvailable()) return;
    const auto encoders=openshot::GpuEncoders();
    if(std::find(encoders.begin(),encoders.end(),"h264_vulkan")==encoders.end()){
        WARN("Skipping Vulkan test: no native Vulkan H.264 encoder");
        return;
    }
    // A different logical device cannot write into the encoder's pool. The
    // original frame is still readable, so this failure can use CPU pixels.
    AVBufferRef* other=nullptr;
    const auto name=openshot::ActiveGpuDevice().name;
    REQUIRE(av_hwdevice_ctx_create(&other,AV_HWDEVICE_TYPE_VULKAN,name.c_str(),nullptr,0)>=0);
    Buffer device(other),pool(av_hwframe_ctx_alloc(other));
    REQUIRE(pool);
    auto* context=reinterpret_cast<AVHWFramesContext*>(pool->data);
    context->format=AV_PIX_FMT_VULKAN;context->sw_format=AV_PIX_FMT_RGBA;
    context->width=256;context->height=256;
    REQUIRE(av_hwframe_ctx_init(pool.get())>=0);
    Frame cpu(av_frame_alloc()),gpu(av_frame_alloc());
    cpu->format=AV_PIX_FMT_RGBA;cpu->width=256;cpu->height=256;
    REQUIRE(av_frame_get_buffer(cpu.get(),0)>=0);
    for(int y=0;y<256;++y)for(int x=0;x<256;++x){
        auto* pixel=cpu->data[0]+y*cpu->linesize[0]+x*4;
        pixel[0]=255;pixel[1]=0;pixel[2]=0;pixel[3]=255;
    }
    REQUIRE(av_hwframe_get_buffer(pool.get(),gpu.get(),0)>=0);
    REQUIRE(av_hwframe_transfer_data(gpu.get(),cpu.get(),0)>=0);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto path=directory.filePath("fallback.mp4").toStdString();
    openshot::FFmpegWriter writer(path);
    writer.SetVideoOptions(true,"h264_vulkan",openshot::Fraction(24,1),256,256,openshot::Fraction(1,1),false,false,1000000);
    writer.Open();
    const auto before=openshot::VulkanFrameReadbackCount();
    for(int n=1;n<=3;++n){
        auto frame=std::make_shared<openshot::Frame>(n,256,256,"black");
        frame->AddDeferredImage(std::make_shared<openshot::VulkanFrameImage>(gpu.get()));
        REQUIRE_NOTHROW(writer.WriteFrame(frame));
        CHECK_FALSE(frame->GetDeferredImage());
    }
    writer.Close();
    CHECK(openshot::VulkanFrameReadbackCount()==before+3);
    openshot::FFmpegReader reader(path);
    reader.Open();
    CHECK(reader.info.video_length==3);
    const auto image=reader.GetFrame(2)->GetImage();
    REQUIRE(image);CHECK(image->pixelColor(128,128).red()>220);
    CHECK(image->pixelColor(128,128).blue()<25);
    reader.Close();
}
TEST_CASE("Vulkan affine surfaces preserve alpha color and ownership", "[vulkan][hardware]") {
    if(!VulkanAvailable()) return;
    AVBufferRef* device_ptr=nullptr;
    REQUIRE(openshot::CreateSharedVulkanDevice(&device_ptr)>=0);
    Buffer device(device_ptr);
    Buffer pool(av_hwframe_ctx_alloc(device.get()));
    REQUIRE(pool);
    auto* context=reinterpret_cast<AVHWFramesContext*>(pool->data);
    context->format=AV_PIX_FMT_VULKAN;context->sw_format=AV_PIX_FMT_RGBA;
    context->width=16;context->height=16;
    REQUIRE(av_hwframe_ctx_init(pool.get())>=0);
    Frame cpu(av_frame_alloc()),gpu(av_frame_alloc());
    REQUIRE(cpu);REQUIRE(gpu);
    cpu->format=AV_PIX_FMT_RGBA;cpu->width=16;cpu->height=16;
    REQUIRE(av_frame_get_buffer(cpu.get(),0)>=0);
    for(int y=0;y<16;++y)for(int x=0;x<16;++x){
        auto* p=cpu->data[0]+y*cpu->linesize[0]+x*4;
        p[0]=255;p[1]=0;p[2]=0;p[3]=255;
    }
    REQUIRE(av_hwframe_get_buffer(pool.get(),gpu.get(),0)>=0);
    REQUIRE(av_hwframe_transfer_data(gpu.get(),cpu.get(),0)>=0);
    auto source=std::make_shared<openshot::VulkanFrameImage>(gpu.get());
    CHECK(source->GetBytes() >= 16 * 16 * 4);
    QTransform transform;transform.translate(8,8);
    auto first=openshot::RenderVulkanImage(source,nullptr,32,32,transform,0.5,Qt::blue);
    // A second render must not overwrite the first cached frame's GPU storage.
    auto second=openshot::RenderVulkanImage(source,nullptr,32,32,QTransform(),1.0,Qt::green);
    gpu.reset();pool.reset();device.reset();source.reset();
    CHECK(first->GetBytes() >= 32 * 32 * 4);
    QImage image=first->Materialize();
    // FFmpeg may import the upload memory asynchronously; retain it until completion.
    cpu.reset();
    const auto blended=image.pixelColor(12,12);
    CHECK(blended.red()>=126);CHECK(blended.red()<=129);
    CHECK(blended.green()==0);
    CHECK(blended.blue()>=126);CHECK(blended.blue()<=129);
    CHECK(blended.alpha()==255);
    CHECK(image.pixelColor(0,0)==QColor(Qt::blue));
    CHECK(second->Materialize().pixelColor(0,0)==QColor(Qt::red));
    // Export conversion remains on the GPU and produces limited-range NV12 (BT.601 at this SD size).
    auto encoded = openshot::ConvertVulkanToNV12(second);
    auto encoded_surface = std::dynamic_pointer_cast<const openshot::VulkanFrameImage>(encoded);
    REQUIRE(encoded_surface);
    // NVENC still receives separate Y/UV Vulkan images for CUDA interop.
    const auto* planes=reinterpret_cast<const AVVkFrame*>(encoded_surface->GetFrame()->data[0]);
    REQUIRE(planes->img[0]!=VK_NULL_HANDLE);
    CHECK(planes->img[1]!=VK_NULL_HANDLE);
    CHECK(planes->img[0]!=planes->img[1]);
    Frame downloaded(av_frame_alloc());
    REQUIRE(downloaded);
#ifdef __APPLE__
    REQUIRE(openshot::ReadMacVulkanFrame(downloaded.get(), encoded_surface->GetFrame()) >= 0);
#else
    REQUIRE(av_hwframe_transfer_data(downloaded.get(), encoded_surface->GetFrame(), 0) >= 0);
#endif
    REQUIRE(downloaded->format == AV_PIX_FMT_NV12);
    const int y = downloaded->data[0][4 * downloaded->linesize[0] + 4];
    const int u = downloaded->data[1][2 * downloaded->linesize[1] + 4];
    const int v = downloaded->data[1][2 * downloaded->linesize[1] + 5];
    CHECK(y >= 80); CHECK(y <= 83);
    CHECK(u >= 89); CHECK(u <= 92);
    CHECK(v >= 238); CHECK(v <= 241);
    const auto converted_red = encoded->Materialize().pixelColor(4, 4);
    CHECK(converted_red.red() >= 252);
    CHECK(converted_red.green() <= 3);
    CHECK(converted_red.blue() <= 3);
    // Exercise the actual Clip transform path against its QPainter fallback.
    // Compare flat interior pixels; edge resampling intentionally differs.
    for(auto mode : {openshot::SCALE_FIT,openshot::SCALE_CROP,openshot::SCALE_STRETCH,openshot::SCALE_NONE}) {
        for(int gravity=openshot::GRAVITY_TOP_LEFT;gravity<=openshot::GRAVITY_BOTTOM_RIGHT;++gravity) {
            PatternReader cpu_reader(second,false),gpu_reader(second,true);
            openshot::Clip cpu_clip(&cpu_reader),gpu_clip(&gpu_reader);
            for(auto* clip : {&cpu_clip,&gpu_clip}) {
                clip->scale=mode;
                clip->gravity=static_cast<openshot::GravityType>(gravity);
                clip->scale_x=openshot::Keyframe(0.7);clip->scale_y=openshot::Keyframe(0.6);
                clip->shear_x=openshot::Keyframe(0.12);clip->shear_y=openshot::Keyframe(-0.08);
                clip->rotation=openshot::Keyframe(13.0);clip->alpha=openshot::Keyframe(0.6);
                clip->Open();
            }
            auto cpu_background=std::make_shared<openshot::Frame>(1,80,60,"#00000000");
            auto gpu_background=std::make_shared<openshot::Frame>(1,80,60,"#00000000");
            auto cpu_frame=cpu_clip.GetFrame(cpu_background,1);
            auto gpu_frame=gpu_clip.GetFrame(gpu_background,1);
            REQUIRE(gpu_frame->GetDeferredImage());
            auto expected=cpu_frame->GetImage();auto actual=gpu_frame->GetImage();
            REQUIRE(expected->size()==actual->size());
            int compared=0,max_error=0;
            for(int y=2;y<expected->height()-2;++y)for(int x=2;x<expected->width()-2;++x) {
                const auto pixel=expected->pixel(x,y);
                if(qAlpha(pixel)==0) continue;
                bool interior=true;
                for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)
                    interior &= expected->pixel(x+dx,y+dy)==pixel;
                if(!interior) continue;
                const auto* a=actual->constScanLine(y)+4*x;
                const auto* b=expected->constScanLine(y)+4*x;
                for(int channel=0;channel<4;++channel)max_error=std::max(max_error,std::abs(int(a[channel])-int(b[channel])));
                ++compared;
            }
            INFO("scale=" << mode << " gravity=" << gravity << " compared=" << compared << " max_error=" << max_error);
            CHECK(compared>8);CHECK(max_error<=3);
            cpu_clip.Close();gpu_clip.Close();
        }
    }
    // Premultiplied output is not multiplied twice when materialized.
    auto transparent=openshot::RenderVulkanImage(second,nullptr,32,32,QTransform(),0.5);
    auto rgba=transparent->Materialize();
    const auto red=rgba.pixelColor(0,0);
    CHECK(red.red()>=253);
    CHECK(red.alpha()>=126);CHECK(red.alpha()<=129);
}

TEST_CASE("Vulkan sequential decoding preserves pending audio video frames", "[vulkan][hardware][reader]") {
    if(!VulkanAvailable()) return;
    // OPENSHOT_VULKAN_TEST_VIDEO may select a larger (e.g. 4K) audio/video clip
    // of at least 120 frames; the bundled trailer covers the same cache paths.
    std::string video=TEST_MEDIA_PATH "sintel_trailer-720p.mp4";
    if(const char* custom=std::getenv("OPENSHOT_VULKAN_TEST_VIDEO"); custom && *custom) video=custom;
    // Full-resolution RGBA frames exceed a small working-cache byte cap before
    // their audio arrives. Evicting those pending frames silently produced black
    // images and incorrect audio; compare sequential reads against direct seeks.
    std::map<int,std::shared_ptr<QImage>> reference;
    {
        openshot::FFmpegReader reader(video);
        reader.Open();
        for(int n : {12,120}) {
            auto frame=reader.GetFrame(n);
            REQUIRE(frame->GetDeferredImage());
            reference[n]=frame->GetImage();
        }
        reader.Close();
    }
    openshot::FFmpegReader reader(video);
    reader.Open();
    for(int n=1;n<=120;++n) {
        auto frame=reader.GetFrame(n);
        REQUIRE(frame->GetDeferredImage());
        if(reference.count(n)) {
            auto actual=frame->GetImage();
            const auto expected=reference[n];
            REQUIRE(actual->size()==expected->size());
            REQUIRE(actual->format()==expected->format());
            uint64_t error=0;
            for(int y=0;y<actual->height();++y) {
                const auto* a=actual->constScanLine(y);
                const auto* b=expected->constScanLine(y);
                for(int x=0;x<actual->width()*4;++x) error+=std::abs(int(a[x])-int(b[x]));
            }
            const double mae=double(error)/(actual->width()*actual->height()*4.0);
            INFO("frame=" << n << " sequential/direct MAE=" << mae);
            CHECK(mae<=3.0);
        }
    }
    reader.Close();
}

TEST_CASE("Vulkan high quality reader resizing uses bicubic antialiasing", "[vulkan][hardware][quality]") {
    if(!VulkanAvailable()) return;
    AVBufferRef* raw_device=nullptr;
    REQUIRE(openshot::CreateSharedVulkanDevice(&raw_device)>=0);
    Buffer device(raw_device),pool(av_hwframe_ctx_alloc(raw_device));
    REQUIRE(pool);
    auto* context=reinterpret_cast<AVHWFramesContext*>(pool->data);
    context->format=AV_PIX_FMT_VULKAN;context->sw_format=AV_PIX_FMT_RGBA;
    context->width=context->height=32;
    REQUIRE(av_hwframe_ctx_init(pool.get())>=0);
    Frame cpu(av_frame_alloc()),gpu(av_frame_alloc());
    REQUIRE(cpu);REQUIRE(gpu);
    cpu->format=AV_PIX_FMT_RGBA;cpu->width=cpu->height=32;
    REQUIRE(av_frame_get_buffer(cpu.get(),0)>=0);
    for(int y=0;y<32;++y)for(int x=0;x<32;++x){
        auto* pixel=cpu->data[0]+y*cpu->linesize[0]+x*4;
        pixel[0]=32+192*((x/4+y/4)%2);pixel[1]=y*8;pixel[2]=x*8;pixel[3]=255;
    }
    REQUIRE(av_hwframe_get_buffer(pool.get(),gpu.get(),0)>=0);
    REQUIRE(av_hwframe_transfer_data(gpu.get(),cpu.get(),0)>=0);
    openshot::VulkanFrameProcessor processor;
    for(const auto size : {QSize(12,10),QSize(61,47),QSize(32,32)}) {
        QImage reference(size,QImage::Format_RGBA8888);
        auto* scaler=sws_getContext(32,32,AV_PIX_FMT_RGBA,size.width(),size.height(),AV_PIX_FMT_RGBA,
                                   SWS_BICUBIC,nullptr,nullptr,nullptr);
        REQUIRE(scaler);
        std::unique_ptr<SwsContext,decltype(&sws_freeContext)> owner(scaler,sws_freeContext);
        uint8_t* output[]={reference.bits(),nullptr,nullptr,nullptr};
        int strides[]={reference.bytesPerLine(),0,0,0};
        REQUIRE(sws_scale(scaler,cpu->data,cpu->linesize,0,32,output,strides)==size.height());
        auto high=processor.Process(gpu.get(),size.width(),size.height(),openshot::color::SourceColor(),true);
        REQUIRE(std::dynamic_pointer_cast<const openshot::VulkanFrameImage>(high));
        auto high_image=high->Materialize();
        auto low_image=processor.Process(gpu.get(),size.width(),size.height(),openshot::color::SourceColor(),false)->Materialize();
        double high_error=0,low_error=0;
        for(int y=0;y<size.height();++y)for(int x=0;x<size.width();++x)for(int channel=0;channel<3;++channel){
            const int expected=reference.constScanLine(y)[4*x+channel];
            high_error+=std::abs(int(high_image.constScanLine(y)[4*x+channel])-expected);
            low_error+=std::abs(int(low_image.constScanLine(y)[4*x+channel])-expected);
        }
        high_error/=size.width()*size.height()*3.0;
        low_error/=size.width()*size.height()*3.0;
        INFO("size=" << size.width() << "x" << size.height() << " bicubicMAE=" << high_error << " bilinearMAE=" << low_error);
        CHECK(high_error<4.0);
        if(size!=QSize(32,32)) CHECK(high_error<low_error);
    }
}

#ifdef __APPLE__
TEST_CASE("VideoToolbox export shares decoder compositor and encoder surfaces", "[vulkan][hardware][writer][apple]") {
    if (!VulkanAvailable()) return;
    const auto encoders = openshot::GpuEncoders();
    if (std::find(encoders.begin(), encoders.end(), "h264_videotoolbox") == encoders.end()) {
        WARN("Skipping Vulkan test: no hardware VideoToolbox H.264 encoder");
        return;
    }
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    for (int pass = 0; pass < 2; ++pass) {
        const int width = pass ? 1280 : 640, height = pass ? 720 : 360;
        openshot::FFmpegReader base_reader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
        openshot::FFmpegReader overlay_reader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
        openshot::Clip base(&base_reader), overlay(&overlay_reader);
        openshot::Timeline timeline(width,height,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
        timeline.SetMaxSize(width,height);
        base.Layer(0); overlay.Layer(1);
        base.Start(5.0); overlay.Start(8.0);
        overlay.scale_x=openshot::Keyframe(0.7); overlay.rotation=openshot::Keyframe(12.0);
        overlay.alpha=openshot::Keyframe(0.6);
        timeline.AddClip(&base); timeline.AddClip(&overlay); timeline.Open();
        const auto path=directory.filePath(QString("videotoolbox-%1.mp4").arg(pass)).toStdString();
        openshot::FFmpegWriter writer(path);
        writer.SetVideoOptions(true,"h264_videotoolbox",openshot::Fraction(24,1),width,height,openshot::Fraction(1,1),false,false,1000000);
        writer.Open();
        std::shared_ptr<openshot::Frame> expected_frame;
        const auto readbacks=openshot::VulkanFrameReadbackCount();
        for(int n=1;n<=24;++n){
            auto frame=timeline.GetFrame(n);
            REQUIRE(frame->GetDeferredImage());
            writer.WriteFrame(frame);
            REQUIRE(frame->GetDeferredImage());
            if (n == 12) expected_frame = frame;
        }
        writer.Close();
        CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
        CHECK(base_reader.HardwareDecodeSuccessful());
        CHECK(overlay_reader.HardwareDecodeSuccessful());
        timeline.Close();
        openshot::FFmpegReader verify(path);
        verify.Open();
        CHECK(verify.info.width==width); CHECK(verify.info.height==height);
        CHECK(verify.info.video_length==24);
        auto image=verify.GetFrame(12)->GetImage();
        REQUIRE(image); CHECK(image->width()==width); CHECK(image->height()==height);
        REQUIRE(expected_frame);
        const auto expected = expected_frame->GetImage();
        REQUIRE(expected);
        double error = 0, brightness = 0;
        int samples = 0;
        for (int y = 8; y < height; y += 16) {
            for (int x = 8; x < width; x += 16) {
                const auto a = expected->pixelColor(x, y), b = image->pixelColor(x, y);
                error += std::abs(a.red()-b.red()) + std::abs(a.green()-b.green()) + std::abs(a.blue()-b.blue());
                brightness += a.red() + a.green() + a.blue();
                samples += 3;
            }
        }
        CHECK(brightness / samples > 10);
        CHECK(error / samples < 20);
        verify.Close();
    }
}
#endif

TEST_CASE("CPU image uploads cache pixels and preserve alpha ownership", "[vulkan][hardware][upload]") {
    if (!VulkanAvailable()) return;
    openshot::ReleaseVulkanUploads();
    QImage image(16, 16, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(200, 60, 20, 128));
    openshot::ResetGpuMetrics();
    std::shared_ptr<const openshot::FrameImage> first, second;
    {
        openshot::gpu_metrics::Scope scope(false);
        first = openshot::UploadVulkanImage(image);
#ifdef __APPLE__
        auto mac_surface = std::dynamic_pointer_cast<const openshot::VulkanFrameImage>(first);
        REQUIRE(mac_surface);
        CHECK(openshot::IsMacVulkanFrame(mac_surface->GetFrame()));
#endif
        CHECK(openshot::UploadVulkanImage(image) == first);
        image.fill(QColor(0, 255, 0, 255));
        second = openshot::UploadVulkanImage(image);
        CHECK(second != first);
    }
    Json::Reader reader;
    Json::Value metrics;
    REQUIRE(reader.parse(openshot::GpuMetricsSnapshot(), metrics));
    CHECK(metrics["uploads"].asUInt64() == 2);
    QTransform identity;
    auto rendered = openshot::RenderVulkanImage(first, nullptr, 16, 16, identity, 1.0, Qt::transparent);
    auto pixel = rendered->Materialize().pixelColor(8,8);
    CHECK(std::abs(pixel.red() - 200) <= 2);
    CHECK(std::abs(pixel.green() - 60) <= 2);
    CHECK(std::abs(pixel.alpha() - 128) <= 1);
    openshot::ReleaseVulkanUploads();
    const auto second_pixel = second->Materialize().pixelColor(8,8);
    INFO("Uploaded opaque RGBA: " << second_pixel.red() << "," << second_pixel.green()
         << "," << second_pixel.blue() << "," << second_pixel.alpha());
    CHECK(second_pixel == QColor(Qt::green));
    CHECK(openshot::UploadVulkanImage(image) != second);
}

TEST_CASE("PNG clips upload once and keep animated transforms on Vulkan", "[vulkan][hardware][upload]") {
    if (!VulkanAvailable()) return;
    QTemporaryDir temporary;
    REQUIRE(temporary.isValid());
    const auto path = temporary.filePath("artwork.png");
    QImage bitmap(64, 64, QImage::Format_ARGB32_Premultiplied);
    bitmap.fill(QColor(0, 0, 255, 128));
    REQUIRE(bitmap.save(path));
    openshot::Clip clip(path.toStdString());
    clip.scale_x.AddPoint(1,0.6,openshot::LINEAR);
    clip.scale_x.AddPoint(8,0.8,openshot::LINEAR);
    clip.scale_y = openshot::Keyframe(0.6);
    clip.rotation.AddPoint(1,10,openshot::LINEAR);
    clip.rotation.AddPoint(8,20,openshot::LINEAR);
    openshot::Saturation saturation;
    saturation.saturation.AddPoint(1,1.0,openshot::LINEAR);
    saturation.saturation.AddPoint(8,1.5,openshot::LINEAR);
    openshot::Crop crop;
    crop.left.AddPoint(1,0.0,openshot::LINEAR);
    crop.left.AddPoint(8,0.125,openshot::LINEAR);
    clip.AddEffect(&saturation);
    clip.AddEffect(&crop);
    openshot::Timeline timeline(128,128,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    openshot::ResetGpuMetrics();
    std::shared_ptr<openshot::Frame> result;
    for (int i = 1; i <= 8; ++i) {
        result = timeline.GetFrame(i);
        REQUIRE(result->GetDeferredImage());
    }
    Json::Reader reader;
    Json::Value metrics;
    REQUIRE(reader.parse(openshot::GpuMetricsSnapshot(), metrics));
    CHECK(metrics["uploads"].asUInt64() == 1);
    CHECK(metrics["readbacks"].asUInt64() == 0);
    CHECK(metrics["render_cpu"].asUInt64() == 0);
    CHECK(metrics["render_gpu"].asUInt64() > 0);
    CHECK(metrics["image_sources"].asUInt64() == 8);
    CHECK(metrics["decode_cpu"].asUInt64() == 0);
    CHECK(metrics["copy_free"].asUInt64() == 8);
    openshot::Frame copy(*result);
    const auto pixel = copy.GetImage()->pixelColor(64,64);
    CHECK(std::abs(pixel.blue() - 128) <= 2);
    CHECK(pixel.red() == 0);
    CHECK(pixel.alpha() == 255);
    timeline.Close();
}

TEST_CASE("FFmpeg still images and cover art upload once", "[vulkan][hardware][upload]") {
    if (!VulkanAvailable()) return;
    QTemporaryDir temporary;
    REQUIRE(temporary.isValid());
    const auto path = temporary.filePath("cover.jpg");
    QImage bitmap(64, 64, QImage::Format_RGB32);
    bitmap.fill(QColor(255, 0, 0));
    REQUIRE(bitmap.save(path));
    // FFmpegReader fills each frame of a still image from the previous one, as
    // it does for an audio file's cover art; that must not upload every frame.
    openshot::FFmpegReader reader(path.toStdString());
    REQUIRE(reader.info.has_single_image);
    openshot::Clip clip(&reader);
    openshot::Timeline timeline(128,128,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    openshot::ResetGpuMetrics();
    for (int i = 1; i <= 24; ++i)
        REQUIRE(timeline.GetFrame(i)->GetDeferredImage());
    Json::Reader parser;
    Json::Value metrics;
    REQUIRE(parser.parse(openshot::GpuMetricsSnapshot(), metrics));
    CHECK(metrics["uploads"].asUInt64() <= 1);
    CHECK(metrics["upload_failures"].asUInt64() == 0);
    timeline.Close();
}

TEST_CASE("Saved GPU preferences reach the native runtime environment", "[vulkan][settings]") {
    struct Restore {
        std::string backend, device;
        ~Restore() { openshot::ConfigureGpuEnvironment(backend, device); }
    } restore{std::getenv("OPENSHOT_RENDER_BACKEND") ? std::getenv("OPENSHOT_RENDER_BACKEND") : "",
              std::getenv("OPENSHOT_VULKAN_DEVICE") ? std::getenv("OPENSHOT_VULKAN_DEVICE") : ""};
    openshot::ConfigureGpuEnvironment("cpu", "Example GPU");
    REQUIRE(std::getenv("OPENSHOT_RENDER_BACKEND"));
    REQUIRE(std::getenv("OPENSHOT_VULKAN_DEVICE"));
    CHECK(std::string(std::getenv("OPENSHOT_RENDER_BACKEND")) == "cpu");
    CHECK(std::string(std::getenv("OPENSHOT_VULKAN_DEVICE")) == "Example GPU");
    CHECK_FALSE(openshot::VulkanRenderingEnabled());
    openshot::ConfigureGpuEnvironment("vulkan", "");
    CHECK(std::string(std::getenv("OPENSHOT_RENDER_BACKEND")) == "vulkan");
    CHECK(std::getenv("OPENSHOT_VULKAN_DEVICE") == nullptr);
}

namespace {
void ProcessCpuEffect(openshot::EffectBase& effect, std::shared_ptr<openshot::Frame> frame, int number) {
    struct Restore {
        std::string backend, device;
        ~Restore() { openshot::ConfigureGpuEnvironment(backend,device); }
    } restore{std::getenv("OPENSHOT_RENDER_BACKEND") ? std::getenv("OPENSHOT_RENDER_BACKEND") : "",
              std::getenv("OPENSHOT_VULKAN_DEVICE") ? std::getenv("OPENSHOT_VULKAN_DEVICE") : ""};
    openshot::ConfigureGpuEnvironment("cpu",restore.device);
    effect.ProcessFrame(frame,number);
}
void CheckVulkanEffect(openshot::EffectBase& effect, const QImage& pixels,
    std::shared_ptr<const openshot::FrameImage> source, int number, int tolerance, bool check_uploads = false) {
    auto cpu=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
    auto gpu=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
    cpu->AddImage(std::make_shared<QImage>(pixels.copy()));
    gpu->AddDeferredImage(source);
    ProcessCpuEffect(effect,cpu,number);
    const auto readbacks=openshot::VulkanFrameReadbackCount();
    const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
    {
        openshot::gpu_metrics::Scope scope(false);
        effect.ProcessFrame(gpu,number);
    }
    if (check_uploads) {
        const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
        CHECK(after["uploads"]==before["uploads"]);
        CHECK(after["readbacks"]==before["readbacks"]);
    }
    REQUIRE(gpu->GetDeferredImage());
    CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
    auto expected=std::make_shared<QImage>(cpu->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied));
    auto actual=gpu->GetImage();
    REQUIRE(actual->size()==expected->size());
    int max_error=0;
    for (int y=0;y<actual->height();++y)
        for (int x=0;x<actual->width()*4;++x)
            max_error=std::max(max_error,std::abs(int(expected->constScanLine(y)[x])-int(actual->constScanLine(y)[x])));
    INFO("frame=" << number << " max byte error=" << max_error);
    CHECK(max_error<=tolerance);
}

}

TEST_CASE("Vulkan saturation matches CPU for animated channels and alpha", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(32, 16, QImage::Format_RGBA8888_Premultiplied);
    for (int y=0; y<pixels.height(); ++y)
        for (int x=0; x<pixels.width(); ++x)
            pixels.setPixelColor(x,y,QColor((x*37)%256,(y*53)%256,(x*19+y*11)%256,(x%4)*85));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::Saturation effect;
    effect.saturation.AddPoint(1,0.0,openshot::LINEAR);
    effect.saturation.AddPoint(9,2.0,openshot::LINEAR);
    effect.saturation_R=openshot::Keyframe(0.7);
    effect.saturation_G=openshot::Keyframe(1.2);
    effect.saturation_B=openshot::Keyframe(0.4);
    for (int number : {1,5,9,3}) {
        CheckVulkanEffect(effect,pixels,source,number,2);
    }
}

TEST_CASE("Vulkan crop matches CPU for offsets resizing and animation", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(32,24,QImage::Format_RGBA8888_Premultiplied);
    for (int y=0; y<24; ++y)
        for (int x=0; x<32; ++x)
            pixels.setPixelColor(x,y,QColor(x*7,y*9,100,(x%4)*85));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::Crop effect;
    effect.left.AddPoint(1,0.125,openshot::LINEAR);
    effect.left.AddPoint(9,0.375,openshot::LINEAR);
    effect.top=openshot::Keyframe(0.25);
    effect.right=openshot::Keyframe(0.125);
    effect.bottom=openshot::Keyframe(0.125);
    for (bool resize : {false,true}) {
        effect.resize=resize;
        for (double shift : {-0.25,0.0,0.25,0.13}) {
            effect.x=openshot::Keyframe(shift);
            effect.y=openshot::Keyframe(shift);
            for (int number : {1,5,9,3}) {
                CheckVulkanEffect(effect,pixels,source,number,0);
            }
        }
    }
}

TEST_CASE("Vulkan grading matches animated CPU controls with transparency", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(32,16,QImage::Format_RGBA8888_Premultiplied);
    for (int y=0; y<16; ++y)
        for (int x=0; x<32; ++x)
            pixels.setPixelColor(x,y,QColor((x*37)%256,(y*53)%256,(x*19+y*11)%256,(x%4)*85));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::ColorGrade effect;
    effect.temperature=openshot::Keyframe(0.4);
    effect.tint=openshot::Keyframe(-0.3);
    effect.exposure.AddPoint(1,-1.0,openshot::LINEAR);
    effect.exposure.AddPoint(9,1.0,openshot::LINEAR);
    effect.contrast=openshot::Keyframe(0.25);
    effect.shadows=openshot::Keyframe(0.3);
    effect.highlights=openshot::Keyframe(-0.4);
    effect.saturation=openshot::Keyframe(1.2);
    effect.vibrance=openshot::Keyframe(0.5);
    effect.mix=openshot::Keyframe(0.7);
    for (int number : {1,5,9,3}) {
        CheckVulkanEffect(effect,pixels,source,number,1);
    }
}

TEST_CASE("Vulkan grading keeps animated wheels and curves on the GPU", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(32,16,QImage::Format_RGBA8888_Premultiplied);
    for(int y=0;y<16;++y)
        for(int x=0;x<32;++x)
            pixels.setPixelColor(x,y,QColor((x*37)%256,(y*53)%256,(x*19+y*11)%256,(x%4)*85));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::ColorGrade effect;
    effect.wheels.global.color=openshot::Color("#ff3040");
    effect.wheels.global.amount.AddPoint(1,-0.3,openshot::LINEAR);
    effect.wheels.global.amount.AddPoint(9,0.4,openshot::LINEAR);
    effect.wheels.shadows.color=openshot::Color("#30ff40");
    effect.wheels.shadows.amount=openshot::Keyframe(0.3);
    effect.wheels.midtones.color=openshot::Color("#3040ff");
    effect.wheels.midtones.amount=openshot::Keyframe(0.25);
    effect.wheels.highlights.luma=openshot::Keyframe(-0.15);
    effect.curve_all.Nodes().emplace_back(2,0.5,0.65,openshot::LINEAR);
    effect.curve_all.Nodes().back().y.AddPoint(1,0.65,openshot::LINEAR);
    effect.curve_all.Nodes().back().y.AddPoint(9,0.35,openshot::LINEAR);
    effect.curve_red.Nodes().back().y=openshot::Keyframe(0.8);
    effect.curve_green.Nodes().back().y=openshot::Keyframe(0.9);
    effect.curve_blue.Nodes().emplace_back(2,0.5,0.35,openshot::LINEAR);
    for(bool enabled : {false,true}) {
        effect.wheels.enabled=openshot::Keyframe(enabled?1.0:0.0);
        effect.curve_blue.enabled=openshot::Keyframe(enabled?1.0:0.0);
        for(int number : {1,5,9,3}) CheckVulkanEffect(effect,pixels,source,number,2);
    }
}

TEST_CASE("Vulkan reader masks match animated CPU wipes and inversion", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QTemporaryDir directory;
    const auto path=directory.filePath("mask.png");
    QImage pixels(32,16,QImage::Format_RGBA8888_Premultiplied);
    for (int y=0; y<16; ++y)
        for (int x=0; x<32; ++x)
            pixels.setPixelColor(x,y,QColor(x*7,y*13,100,(x%4)*85));
    REQUIRE(pixels.save(path));
    openshot::QtImageReader reader(path.toStdString());
    openshot::Mask effect(&reader,openshot::Keyframe(0.0),openshot::Keyframe(3.0));
    effect.brightness.AddPoint(1,-0.5,openshot::LINEAR);
    effect.brightness.AddPoint(9,0.5,openshot::LINEAR);
    auto source=openshot::UploadVulkanImage(pixels);
    for (bool invert : {false,true}) {
        effect.mask_invert=invert;
        for (bool replace : {false,true}) {
            effect.replace_image=replace;
            for (int number : {1,5,9}) {
                CheckVulkanEffect(effect,pixels,source,number,0);
            }
        }
    }
}

TEST_CASE("Vulkan blur matches CPU region edges iterations and alpha", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(32,16,QImage::Format_RGBA8888_Premultiplied);
    for (int y=0; y<16; ++y)
        for (int x=0; x<32; ++x)
            pixels.setPixelColor(x,y,QColor((x*37)%256,(y*53)%256,(x*19+y*11)%256,(x%4)*85));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::Blur effect;
    effect.horizontal_radius.AddPoint(1,0.0,openshot::LINEAR);
    effect.horizontal_radius.AddPoint(9,100.0,openshot::LINEAR);
    effect.vertical_radius=openshot::Keyframe(3.0);
    effect.iterations=openshot::Keyframe(2.0);
    for (double margin : {0.0,0.25}) {
        effect.left=effect.right=effect.top=effect.bottom=openshot::Keyframe(margin);
        for (int number : {1,3,9}) {
            CheckVulkanEffect(effect,pixels,source,number,1);
        }
    }
}

TEST_CASE("Animated output sizes reuse Vulkan pipelines and preserve earlier frames", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(48,32,QImage::Format_RGBA8888_Premultiplied);
    pixels.fill(Qt::red);
    auto source=openshot::UploadVulkanImage(pixels);
    const auto created=openshot::VulkanPipelineCreationCount();
    std::vector<std::shared_ptr<const openshot::FrameImage>> frames;
    for(int width=16;width<48;++width) {
        frames.push_back(openshot::RenderVulkanImage(source,nullptr,width,32,QTransform(),1.0));
        REQUIRE(frames.back()->GetWidth()==width);
    }
    CHECK(openshot::VulkanPipelineCreationCount()-created<=1);
    // Old frames remain valid after both submission slots and retained pools turn over.
    CHECK(frames.front()->Materialize().pixelColor(8,16)==QColor(Qt::red));
    CHECK(frames.back()->Materialize().pixelColor(8,16)==QColor(Qt::red));
}

TEST_CASE("Queued grading frames retain their own wheel parameters", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(16,16,QImage::Format_RGBA8888_Premultiplied);
    pixels.fill(QColor(100,80,60));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::VulkanGradeParameters parameters{};
    parameters.controls={0,0,1,1,0,0,1,0,1};
    for(auto& curve:parameters.curves)
        for(int i=0;i<256;++i) curve[i]=float(i)/255.0f;
    std::vector<std::shared_ptr<const openshot::FrameImage>> frames;
    for(int i=0;i<12;++i) {
        parameters.wheels[0]=float(i)/32.0f;
        frames.push_back(openshot::GradeVulkanImage(source,parameters));
    }
    for(int i=0;i<12;++i) {
        const auto color=frames[i]->Materialize().pixelColor(8,8);
        CHECK(std::abs(color.red()-int(100+255.0*i/32.0+0.5))<=1);
        CHECK(color.green()==80);
        CHECK(color.blue()==60);
    }
}

TEST_CASE("Scope analysis preserves the cached GPU preview surface", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels(16,16,QImage::Format_RGBA8888_Premultiplied);
    pixels.fill(Qt::red);
    auto source=openshot::UploadVulkanImage(pixels);
    auto frame=std::make_shared<openshot::Frame>(1,16,16,"transparent");
    frame->AddDeferredImage(source);
    openshot::FrameScope scope;
    scope.SetFrame(frame);
    CHECK(scope.HasVideo());
    CHECK(frame->GetDeferredImage()==source);
}

namespace {
QImage EffectPixels(int width=32,int height=16) {
    QImage pixels(width,height,QImage::Format_RGBA8888_Premultiplied);
    for (int y=0; y<height; ++y)
        for (int x=0; x<width; ++x)
            pixels.setPixelColor(x,y,QColor((x*37)%256,(y*53)%256,(x*19+y*11)%256,(x%4)*85));
    return pixels;
}
// Nonlinear .cube table with a custom input domain.
std::string WriteCube(const QTemporaryDir& directory,bool cube,int size) {
    const auto path=directory.filePath(cube?"grade3d.cube":"grade1d.cube");
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly|QIODevice::Text));
    QTextStream out(&file);
    out << "TITLE \"test\"\n" << (cube?"LUT_3D_SIZE ":"LUT_1D_SIZE ") << size << "\n"
        << "DOMAIN_MIN 0.1 0.0 0.05\nDOMAIN_MAX 0.9 1.0 1.0\n";
    const int entries=cube?size*size*size:size;
    for (int i=0; i<entries; ++i) {
        const float r=float(i%size)/float(size-1);
        const float g=float(cube?(i/size)%size:i)/float(size-1);
        const float b=float(cube?i/(size*size):i)/float(size-1);
        out << r*r << ' ' << std::sqrt(g) << ' ' << (1.0f-0.8f*b) << '\n';
    }
    return path.toStdString();
}
}

TEST_CASE("Vulkan LUTs match CPU ColorMap for 1D and 3D tables", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QTemporaryDir directory;
    const QImage pixels=EffectPixels();
    auto source=openshot::UploadVulkanImage(pixels);
    for (bool cube : {false,true}) {
        openshot::ColorMap effect(WriteCube(directory,cube,cube?5:7));
        effect.intensity.AddPoint(1,0.2,openshot::LINEAR);
        effect.intensity.AddPoint(9,1.0,openshot::LINEAR);
        effect.intensity_g=openshot::Keyframe(0.5);
        for (int number : {1,5,9}) {
            INFO("cube=" << cube);
            CheckVulkanEffect(effect,pixels,source,number,1);
        }
    }
}

TEST_CASE("Vulkan grading applies its LUT without leaving the GPU", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QTemporaryDir directory;
    const QImage pixels=EffectPixels();
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::ColorGrade effect;
    Json::Value settings;
    settings["lut_path"]=WriteCube(directory,true,5);
    settings["lut_intensity"]=openshot::Keyframe(0.8).JsonValue();
    effect.SetJsonValue(settings);
    effect.exposure.AddPoint(1,-0.5,openshot::LINEAR);
    effect.exposure.AddPoint(9,0.5,openshot::LINEAR);
    effect.saturation=openshot::Keyframe(1.3);
    for (int number : {1,5,9}) CheckVulkanEffect(effect,pixels,source,number,2);
    // A neutral grade applies only the LUT.
    effect.exposure=openshot::Keyframe(0.0);
    effect.saturation=openshot::Keyframe(1.0);
    CheckVulkanEffect(effect,pixels,source,1,1);
}

TEST_CASE("Vulkan effect masks blend on the GPU like the CPU", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QTemporaryDir directory;
    const auto path=directory.filePath("mask.png");
    QImage mask(20,10,QImage::Format_RGBA8888_Premultiplied);
    for (int y=0; y<mask.height(); ++y)
        for (int x=0; x<mask.width(); ++x)
            mask.setPixelColor(x,y,QColor(x*13,y*25,(x*y*7)%256));
    REQUIRE(mask.save(path));
    const QImage pixels=EffectPixels();
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::ColorGrade grade;
    grade.exposure=openshot::Keyframe(0.7);
    grade.temperature=openshot::Keyframe(-0.4);
    openshot::Saturation saturation;
    saturation.saturation=openshot::Keyframe(0.0);
    QTemporaryDir tables;
    openshot::ColorMap lut(WriteCube(tables,true,5));
    // Crop is byte exact on its own, so it checks the blend exactly. Metal
    // (MoltenVK) compiles with fast math, which may fuse the blend's
    // multiply-add and round one byte differently.
#ifdef __APPLE__
    const int blend_tolerance=1;
#else
    const int blend_tolerance=0;
#endif
    openshot::Crop crop;
    crop.left=openshot::Keyframe(0.25);
    crop.bottom=openshot::Keyframe(0.3);
    // Saturation and Blur also have a "drive amount" mask mode.
    openshot::Saturation driven_saturation;
    driven_saturation.saturation=openshot::Keyframe(0.0);
    driven_saturation.mask_mode=openshot::SATURATION_MASK_DRIVE_AMOUNT;
    openshot::Blur driven_blur;
    driven_blur.horizontal_radius=openshot::Keyframe(4.0);
    driven_blur.vertical_radius=openshot::Keyframe(2.0);
    driven_blur.mask_mode=openshot::BLUR_MASK_DRIVE_AMOUNT;
    const std::vector<std::pair<openshot::EffectBase*,int>> effects{{&crop,blend_tolerance},{&grade,1},{&saturation,2},{&lut,1},
        {&driven_saturation,2},{&driven_blur,1}};
    for (const auto& [effect,tolerance] : effects) {
        effect->MaskReader(new openshot::QtImageReader(path.toStdString()));
        for (bool invert : {false,true}) {
            effect->mask_invert=invert;
            INFO(effect->info.class_name << " invert=" << invert);
            CheckVulkanEffect(*effect,pixels,source,1,tolerance);
        }
    }
}

TEST_CASE("Vulkan scopes count on the GPU like the CPU", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QImage pixels=EffectPixels(48,24);
    pixels.setPixelColor(1,1,QColor(255,255,255));
    pixels.setPixelColor(2,1,QColor(0,0,0));
    pixels.setPixelColor(5,1,QColor(255,10,250));
    auto source=openshot::UploadVulkanImage(pixels);
    for (bool region : {false,true}) {
        for (int columns : {20,64}) {
            openshot::FrameScope cpu,gpu;
            for (auto* scope : {&cpu,&gpu}) {
                scope->SetWaveformColumns(columns);
                scope->SetVectorscopeSize(64);
                if (region) scope->SetVideoRegionNormalized(0.2f,0.1f,0.5f,0.7f);
            }
            auto cpu_frame=std::make_shared<openshot::Frame>(1,pixels.width(),pixels.height(),"transparent");
            cpu_frame->AddImage(std::make_shared<QImage>(pixels.copy()));
            auto gpu_frame=std::make_shared<openshot::Frame>(1,pixels.width(),pixels.height(),"transparent");
            gpu_frame->AddDeferredImage(source);
            cpu.SetFrame(cpu_frame);
            const auto readbacks=openshot::VulkanFrameReadbackCount();
            gpu.SetFrame(gpu_frame);
            INFO("region=" << region << " columns=" << columns);
            CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
            CHECK(gpu_frame->GetDeferredImage()==source);
            REQUIRE(gpu.HasVideo());
            CHECK(gpu.GetVideoHistogramLumaBins()==cpu.GetVideoHistogramLumaBins());
            CHECK(gpu.GetVideoHistogramRedBins()==cpu.GetVideoHistogramRedBins());
            CHECK(gpu.GetVideoHistogramGreenBins()==cpu.GetVideoHistogramGreenBins());
            CHECK(gpu.GetVideoHistogramBlueBins()==cpu.GetVideoHistogramBlueBins());
            CHECK(gpu.GetVideoWaveformLumaBins()==cpu.GetVideoWaveformLumaBins());
            CHECK(gpu.GetVideoWaveformRedBins()==cpu.GetVideoWaveformRedBins());
            CHECK(gpu.GetVideoWaveformGreenBins()==cpu.GetVideoWaveformGreenBins());
            CHECK(gpu.GetVideoWaveformBlueBins()==cpu.GetVideoWaveformBlueBins());
            CHECK(gpu.GetVideoVectorscopeBins()==cpu.GetVideoVectorscopeBins());
            CHECK(gpu.GetVideoClippedShadows()==cpu.GetVideoClippedShadows());
            CHECK(gpu.GetVideoClippedHighlights()==cpu.GetVideoClippedHighlights());
            CHECK(gpu.GetVideoClippedRed()==cpu.GetVideoClippedRed());
            CHECK(gpu.GetVideoClippedGreen()==cpu.GetVideoClippedGreen());
            CHECK(gpu.GetVideoClippedBlue()==cpu.GetVideoClippedBlue());
            CHECK(std::abs(gpu.GetVideoAverageLuma()-cpu.GetVideoAverageLuma())<=0.5/255.0+1e-6);
        }
    }
}

TEST_CASE("Masked clip grading with a LUT and scopes stays on Vulkan", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    QTemporaryDir directory;
    const auto artwork=directory.filePath("artwork.png"),mask=directory.filePath("mask.png");
    REQUIRE(EffectPixels(64,64).save(artwork));
    QImage gradient(16,16,QImage::Format_RGBA8888);
    for (int y=0; y<16; ++y)
        for (int x=0; x<16; ++x)
            gradient.setPixelColor(x,y,QColor(x*17,x*17,x*17));
    REQUIRE(gradient.save(mask));
    openshot::Clip clip(artwork.toStdString());
    openshot::ColorGrade grade;
    Json::Value settings;
    settings["lut_path"]=WriteCube(directory,true,5);
    settings["lut_intensity"]=openshot::Keyframe(0.6).JsonValue();
    settings["exposure"]=openshot::Keyframe(0.4).JsonValue();
    grade.SetJsonValue(settings);
    grade.MaskReader(new openshot::QtImageReader(mask.toStdString()));
    clip.AddEffect(&grade);
    openshot::Timeline timeline(128,128,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
    timeline.AddClip(&clip);
    timeline.Open();
    openshot::ResetGpuMetrics();
    for (int i=1; i<=4; ++i) {
        auto frame=timeline.GetFrame(i);
        REQUIRE(frame->GetDeferredImage());
        openshot::FrameScope scope;
        scope.SetVideoRegionNormalized(0.25f,0.25f,0.5f,0.5f);
        scope.SetFrame(frame);
        REQUIRE(scope.HasVideo());
        const auto bins=scope.GetVideoHistogramLumaBins();
        const uint64_t counted=std::accumulate(bins.begin(),bins.end(),uint64_t{0});
        CHECK(counted>0);
    }
    // A neutral grade applies only its LUT, still on the GPU.
    grade.exposure=openshot::Keyframe(0.0);
    for (int i=5; i<=6; ++i) {
        auto frame=timeline.GetFrame(i);
        REQUIRE(frame->GetDeferredImage());
    }
    Json::Reader reader;
    Json::Value metrics;
    REQUIRE(reader.parse(openshot::GpuMetricsSnapshot(), metrics));
    // The clip and the still mask each upload once; nothing returns to the CPU.
    CHECK(metrics["uploads"].asUInt64()==2);
    CHECK(metrics["readbacks"].asUInt64()==0);
    CHECK(metrics["render_cpu"].asUInt64()==0);
    clip.RemoveEffect(&grade);
    timeline.Close();
}

TEST_CASE("Video masks stay on Vulkan for Mask and effect masks", "[vulkan][hardware][effects]") {
    if (!VulkanAvailable()) return;
    // Scale a 720p decoded mask down to the frame on the GPU, for the Mask
    // effect's wipe and for the common effect mask.
    const QImage pixels=EffectPixels(320,180);
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::Mask wipe;
    wipe.MaskReader(new openshot::FFmpegReader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4"));
    openshot::ColorGrade grade;
    grade.exposure=openshot::Keyframe(1.0);
    grade.MaskReader(new openshot::FFmpegReader(TEST_MEDIA_PATH "sintel_trailer-720p.mp4"));
    // These frames have detailed artwork; the trailer opens on black.
    const std::vector<int> numbers{300,700};
    {
        openshot::FFmpegReader probe(TEST_MEDIA_PATH "sintel_trailer-720p.mp4");
        probe.Open();
        for (int number : numbers) {
            auto frame=probe.GetFrame(number);
            REQUIRE(frame->GetDeferredImage());
            const auto image=frame->GetImage();
            int lowest=255,highest=0;
            for (int y=0; y<image->height(); y+=8)
                for (int x=0; x<image->width(); x+=8) {
                    const int gray=qGray(image->pixel(x,y));
                    lowest=std::min(lowest,gray); highest=std::max(highest,gray);
                }
            REQUIRE(highest-lowest>100);
        }
    }
    for (auto* effect : std::initializer_list<openshot::EffectBase*>{&wipe,&grade}) {
        for (int number : numbers) {
            // GPU first: the CPU run reads the reader's cached mask frames back.
            auto gpu=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
            gpu->AddDeferredImage(source);
            const auto readbacks=openshot::VulkanFrameReadbackCount();
            effect->ProcessFrame(gpu,number);
            INFO(effect->info.class_name << " frame=" << number);
            REQUIRE(gpu->GetDeferredImage());
            CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
            auto cpu=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
            cpu->AddImage(std::make_shared<QImage>(pixels.copy()));
            ProcessCpuEffect(*effect,cpu,number);
            // The GPU scales the mask with its own filter, so compare on average.
            auto expected=cpu->GetImage(),actual=gpu->GetImage();
            double total=0.0;
            for (int y=0; y<actual->height(); ++y)
                for (int x=0; x<actual->width()*4; ++x)
                    total+=std::abs(int(expected->constScanLine(y)[x])-int(actual->constScanLine(y)[x]));
            const double mean=total/(actual->width()*actual->height()*4.0);
            double changed=0.0;
            for (int y=0; y<actual->height(); ++y)
                for (int x=0; x<actual->width()*4; ++x)
                    changed+=std::abs(int(pixels.constScanLine(y)[x])-int(actual->constScanLine(y)[x]));
            changed/=actual->width()*actual->height()*4.0;
            INFO("mean byte error=" << mean << " mean change=" << changed);
            CHECK(changed>0.5);
            CHECK(mean<=2.0);
        }
    }
}

TEST_CASE("Vulkan decoding converts SDR, HLG and PQ charts like the CPU path", "[vulkan][hardware][color]") {
    if(!VulkanAvailable()) return;
    struct DecoderSetting {
        int previous=openshot::Settings::Instance()->HARDWARE_DECODER;
        ~DecoderSetting(){openshot::Settings::Instance()->HARDWARE_DECODER=previous;}
    } restore;
    // Mean of a flat patch interior; see tests/generate-color-charts.py.
    auto patch=[](std::shared_ptr<openshot::Frame> frame,int index){
        const int x0=(index%4)*160+70,y0=(index/4)*96+42;
        std::array<long,3> sum{};
        for(int y=y0;y<y0+12;++y)for(int x=x0;x<x0+20;++x)for(int c=0;c<3;++c)
            sum[c]+=frame->GetPixels(y)[x*4+c];
        return std::array<int,3>{int(sum[0]/240),int(sum[1]/240),int(sum[2]/240)};
    };
    for(const char* chart : {"color-chart-sdr-bt709.mp4","color-chart-hlg.mp4","color-chart-pq.mp4"}) {
        const std::string path=std::string(TEST_MEDIA_PATH)+chart;
        // The CPU pipeline is selected per reader when it opens.
        qputenv("OPENSHOT_RENDER_BACKEND","cpu");
        openshot::FFmpegReader cpu_reader(path);
        cpu_reader.Open();
        qunsetenv("OPENSHOT_RENDER_BACKEND");
        auto cpu=cpu_reader.GetFrame(2);
        REQUIRE_FALSE(cpu->GetDeferredImage());
        openshot::Settings::Instance()->HARDWARE_DECODER=1;
        openshot::FFmpegReader gpu_reader(path);
        gpu_reader.Open();
        auto gpu=gpu_reader.GetFrame(2);
        // 10-bit P010 surfaces stay on the GPU instead of falling back to software decode.
#ifdef __APPLE__
        // VideoToolbox decodes H.264 and HEVC Main10 on every supported Mac. Intel
        // Macs refuse very small H.264 frames (320x192), so the charts are 640x384.
        REQUIRE(gpu->GetDeferredImage());
#else
        if(!gpu->GetDeferredImage()) {
            WARN("Skipping Vulkan test: this device does not decode " << chart << " on the GPU");
            continue;
        }
#endif
        for(int i=0;i<13;++i) {
            const auto expected=patch(cpu,i),actual=patch(gpu,i);
            INFO(chart << " patch " << i << " cpu (" << expected[0] << "," << expected[1] << "," << expected[2]
                 << ") gpu (" << actual[0] << "," << actual[1] << "," << actual[2] << ")");
            for(int c=0;c<3;++c) CHECK(std::abs(actual[c]-expected[c])<=3);
        }
    }
}

TEST_CASE("Vulkan export encodes and tags HD with BT.709", "[vulkan][hardware][color]") {
    if(!VulkanAvailable()) return;
    QImage red(1280,720,QImage::Format_RGBA8888_Premultiplied);
    red.fill(QColor(255,0,0));
    auto encoded=std::dynamic_pointer_cast<const openshot::VulkanFrameImage>(
        openshot::ConvertVulkanToNV12(openshot::UploadVulkanImage(red)));
    REQUIRE(encoded);
    const AVFrame* nv12=encoded->GetFrame();
    CHECK(nv12->colorspace==AVCOL_SPC_BT709);
    CHECK(nv12->color_primaries==AVCOL_PRI_BT709);
    CHECK(nv12->color_trc==AVCOL_TRC_BT709);
    CHECK(nv12->color_range==AVCOL_RANGE_MPEG);
    Frame downloaded(av_frame_alloc());
    REQUIRE(downloaded);
#ifdef __APPLE__
    REQUIRE(openshot::ReadMacVulkanFrame(downloaded.get(), nv12) >= 0);
#else
    REQUIRE(av_hwframe_transfer_data(downloaded.get(), nv12, 0) >= 0);
#endif
    // BT.709 limited range red: Y 63, Cb 102, Cr 240 (BT.601 would be 81, 90, 240).
    const int y=downloaded->data[0][100*downloaded->linesize[0]+100];
    const int u=downloaded->data[1][50*downloaded->linesize[1]+100];
    const int v=downloaded->data[1][50*downloaded->linesize[1]+101];
    CHECK(y>=62); CHECK(y<=64);
    CHECK(u>=101); CHECK(u<=103);
    CHECK(v>=239); CHECK(v<=241);
}

#ifdef __APPLE__
namespace {
// A Core Video 4:2:0 biplanar buffer filled with one Y'CbCr value per half
// (left, right), as VideoToolbox returns decoded frames. 10-bit formats keep
// samples in the high bits of 16-bit words, like P010.
struct PixelBuffer {
    CVPixelBufferRef pixel=nullptr;
    PixelBuffer(OSType type,int width,int height,std::array<int,3> left,std::array<int,3> right,int bits) {
        const void* keys[]={kCVPixelBufferIOSurfacePropertiesKey,kCVPixelBufferMetalCompatibilityKey};
        CFDictionaryRef empty=CFDictionaryCreate(kCFAllocatorDefault,nullptr,nullptr,0,
            &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
        const void* values[]={empty,kCFBooleanTrue};
        CFDictionaryRef attributes=CFDictionaryCreate(kCFAllocatorDefault,keys,values,2,
            &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
        const CVReturn created=CVPixelBufferCreate(kCFAllocatorDefault,width,height,type,attributes,&pixel);
        CFRelease(attributes);CFRelease(empty);
        REQUIRE(created==kCVReturnSuccess);
        REQUIRE(CVPixelBufferLockBaseAddress(pixel,0)==kCVReturnSuccess);
        const int shift=bits>8?16-bits:0;
        auto store=[&](uint8_t* row,int x,int value){
            if(bits>8) reinterpret_cast<uint16_t*>(row)[x]=uint16_t(value<<shift);
            else row[x]=uint8_t(value);
        };
        for(int plane=0;plane<2;++plane) {
            auto* base=static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel,plane));
            const size_t stride=CVPixelBufferGetBytesPerRowOfPlane(pixel,plane);
            const int plane_width=int(CVPixelBufferGetWidthOfPlane(pixel,plane));
            const int plane_height=int(CVPixelBufferGetHeightOfPlane(pixel,plane));
            for(int y=0;y<plane_height;++y)for(int x=0;x<plane_width;++x) {
                const auto& value=x<plane_width/2?left:right;
                if(plane==0) store(base+y*stride,x,value[0]);
                else { store(base+y*stride,2*x,value[1]); store(base+y*stride,2*x+1,value[2]); }
            }
        }
        CVPixelBufferUnlockBaseAddress(pixel,0);
    }
    ~PixelBuffer(){ if(pixel) CVPixelBufferRelease(pixel); }
    // An FFmpeg VideoToolbox frame referencing this buffer, as the decoder returns.
    Frame Wrap(AVColorSpace matrix,AVColorTransferCharacteristic trc,AVColorPrimaries primaries) const {
        Frame frame(av_frame_alloc());
        REQUIRE(frame);
        frame->format=AV_PIX_FMT_VIDEOTOOLBOX;
        frame->width=int(CVPixelBufferGetWidth(pixel));frame->height=int(CVPixelBufferGetHeight(pixel));
        frame->data[3]=reinterpret_cast<uint8_t*>(pixel);
        frame->colorspace=matrix;frame->color_trc=trc;frame->color_primaries=primaries;
        frame->color_range=AVCOL_RANGE_MPEG;
        return frame;
    }
};
}

TEST_CASE("Mac VideoToolbox surfaces map to shareable FFmpeg layouts", "[vulkan][hardware][mac][color]") {
    CHECK(openshot::MacSurfaceFormat(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)==AV_PIX_FMT_NV12);
    CHECK(openshot::MacSurfaceFormat(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)==AV_PIX_FMT_NV12);
    CHECK(openshot::MacSurfaceFormat(kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange)==AV_PIX_FMT_P010);
    CHECK(openshot::MacSurfaceFormat(kCVPixelFormatType_420YpCbCr10BiPlanarFullRange)==AV_PIX_FMT_P010);
    CHECK(openshot::MacSurfaceFormat(kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange)==AV_PIX_FMT_P210);
    CHECK(openshot::MacSurfaceFormat(kCVPixelFormatType_32BGRA)==AV_PIX_FMT_NONE);
}

TEST_CASE("Mac 10-bit VideoToolbox surfaces convert on the GPU", "[vulkan][hardware][mac][color]") {
    if(!VulkanAvailable()) return;
    // Left half: BT.709 limited-range red (Y'CbCr 250, 409, 960 at 10 bits).
    // Right half: 75% grey, the HLG reference white (Y' 721, neutral chroma).
    const std::array<int,3> red{250,409,960},grey{721,512,512};
    for(const OSType type : {kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange}) {
        INFO("Core Video format " << type);
        PixelBuffer buffer(type,64,32,red,grey,10);
        openshot::VulkanFrameProcessor processor;

        // SDR BT.709: exercises 16-bit plane mapping and 10-bit scaling.
        auto sdr=buffer.Wrap(AVCOL_SPC_BT709,AVCOL_TRC_BT709,AVCOL_PRI_BT709);
        const auto sdr_color=openshot::color::DescribeFrame(sdr.get(),64,32);
        auto surface=processor.Process(sdr.get(),64,32,sdr_color);
        REQUIRE(std::dynamic_pointer_cast<const openshot::VulkanFrameImage>(surface));
        QImage image=surface->Materialize();
        const QColor left=image.pixelColor(8,16),right=image.pixelColor(56,16);
        INFO("SDR left " << left.name().toStdString() << " right " << right.name().toStdString());
        CHECK(left.red()>=252);CHECK(left.green()<=3);CHECK(left.blue()<=3);
        // 75% grey without HDR conversion: (721-64)/876 = 0.75 -> 191.
        CHECK(std::abs(right.red()-191)<=2);CHECK(std::abs(right.green()-191)<=2);CHECK(std::abs(right.blue()-191)<=2);

        // HLG BT.2020: reference white maps to the tone curve's SDR white.
        auto hlg=buffer.Wrap(AVCOL_SPC_BT2020_NCL,AVCOL_TRC_ARIB_STD_B67,AVCOL_PRI_BT2020);
        const auto hlg_color=openshot::color::DescribeFrame(hlg.get(),64,32);
        REQUIRE(hlg_color.NeedsConversion());
        image=processor.Process(hlg.get(),64,32,hlg_color)->Materialize();
        const int white=int(std::lround(255.0*std::pow(openshot::color::ToneMap(1.0f,hlg_color.RelativePeak()),1.0/2.4)));
        const QColor hlg_grey=image.pixelColor(56,16);
        INFO("HLG grey " << hlg_grey.name().toStdString() << " expected " << white);
        CHECK(std::abs(hlg_grey.red()-white)<=3);CHECK(std::abs(hlg_grey.green()-white)<=3);CHECK(std::abs(hlg_grey.blue()-white)<=3);

        // CPU readback of the shared surface keeps the 10-bit samples.
        Frame imported(openshot::ImportVideoToolboxFrame(sdr.get()));
        REQUIRE(imported);
        Frame readback(av_frame_alloc());
        REQUIRE(readback);
        REQUIRE(openshot::ReadMacVulkanFrame(readback.get(),imported.get())>=0);
        CHECK(readback->format==(type==kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange?AV_PIX_FMT_P010:AV_PIX_FMT_P210));
        CHECK(reinterpret_cast<const uint16_t*>(readback->data[0]+16*readback->linesize[0])[8]==uint16_t(250<<6));
        CHECK(reinterpret_cast<const uint16_t*>(readback->data[0]+16*readback->linesize[0])[56]==uint16_t(721<<6));
    }
}
#endif

namespace {
// Odd dimensions exercise partial workgroups; low alpha exposes differences in
// unpremultiplication and byte truncation that opaque photographs conceal.
QImage PointEffectPixels(int width=37,int height=23) {
    QImage pixels(width,height,QImage::Format_RGBA8888_Premultiplied);
    const std::array<int,11> alpha{0,1,2,3,17,64,127,128,170,254,255};
    for(int y=0;y<height;++y)
        for(int x=0;x<width;++x)
            pixels.setPixelColor(x,y,QColor((x*37+y*11)%256,(y*53+x*7)%256,
                (x*19+y*29)%256,alpha[(x+y)%alpha.size()]));
    return pixels;
}
}

TEST_CASE("Vulkan brightness matches CPU clipping animation and low alpha", "[vulkan][hardware][effects][point-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels();
    const auto source=openshot::UploadVulkanImage(pixels);
    openshot::Brightness effect;
    effect.brightness.AddPoint(1,-1.0,openshot::LINEAR);
    effect.brightness.AddPoint(9,1.0,openshot::LINEAR);
    for(double contrast : {-128.0,0.0,3.0,128.0}) {
        effect.contrast=openshot::Keyframe(contrast);
        for(int number : {1,3,5,9,2}) {
            INFO("contrast=" << contrast);
            CheckVulkanEffect(effect,pixels,source,number,1,true);
        }
    }
}

TEST_CASE("Vulkan hue matches CPU rotations animation and low alpha", "[vulkan][hardware][effects][point-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels();
    const auto source=openshot::UploadVulkanImage(pixels);
    openshot::Hue effect;
    effect.hue.AddPoint(1,-1.0,openshot::LINEAR);
    effect.hue.AddPoint(17,1.0,openshot::LINEAR);
    for(int number : {1,3,5,9,11,15,17,4})
        CheckVulkanEffect(effect,pixels,source,number,1,true);
}

TEST_CASE("Vulkan negate matches Qt inversion with low alpha", "[vulkan][hardware][effects][point-effects]") {
    if(!VulkanAvailable()) return;
    openshot::Negate effect;
    for(const QSize size : {QSize(37,23),QSize(1,1),QSize(1,29)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());
        CheckVulkanEffect(effect,pixels,openshot::UploadVulkanImage(pixels),1,1,true);
    }
}

TEST_CASE("Vulkan bars match CPU edge rounding clamping and animated colors", "[vulkan][hardware][effects][point-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size : {QSize(37,23),QSize(1,1),QSize(1,29)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());
        const auto source=openshot::UploadVulkanImage(pixels);
        openshot::Bars effect;
        effect.color=openshot::Color("#ab369e");
        effect.color.alpha.AddPoint(1,0,openshot::LINEAR);
        effect.color.alpha.AddPoint(9,255,openshot::LINEAR);
        for(double margin : {-0.2,0.0,0.01,0.1,0.5,1.0,1.2}) {
            effect.top=openshot::Keyframe(margin);
            effect.left=openshot::Keyframe(margin/2);
            effect.right=openshot::Keyframe(margin/3);
            effect.bottom=openshot::Keyframe(margin/4);
            for(int number : {1,5,9})
                CheckVulkanEffect(effect,pixels,source,number,0,true);
        }
    }
}

TEST_CASE("Vulkan shifts match CPU wrapping rounding and independent RGBA", "[vulkan][hardware][effects][point-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size : {QSize(37,23),QSize(1,1),QSize(1,29)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());
        const auto source=openshot::UploadVulkanImage(pixels);
        openshot::Shift shift;
        openshot::ColorShift channels;
        for(double offset : {-2.25,-1.0,-0.9999,-0.5,-0.013,0.0,0.013,0.5,0.9999,1.0,2.25}) {
            INFO("offset=" << offset << " size=" << size.width() << "x" << size.height());
            shift.x=openshot::Keyframe(offset);
            shift.y=openshot::Keyframe(-offset);
            channels.red_x=channels.alpha_y=openshot::Keyframe(offset);
            channels.green_y=channels.alpha_x=openshot::Keyframe(-offset);
            channels.blue_x=openshot::Keyframe(offset/2);
            channels.red_y=openshot::Keyframe(offset/3);
            CheckVulkanEffect(shift,pixels,source,1,0,true);
            CheckVulkanEffect(channels,pixels,source,1,0,true);
        }
        channels.green_x.AddPoint(1,-0.7,openshot::LINEAR);
        channels.green_x.AddPoint(9,0.7,openshot::LINEAR);
        shift.x.AddPoint(1,-0.7,openshot::LINEAR);
        shift.x.AddPoint(9,0.7,openshot::LINEAR);
        for(int number : {1,5,9,3}) {
            CheckVulkanEffect(channels,pixels,source,number,0,true);
            CheckVulkanEffect(shift,pixels,source,number,0,true);
        }
    }
}

TEST_CASE("Vulkan point effect masks match CPU including vary strength", "[vulkan][hardware][effects][point-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels();
    const auto source=openshot::UploadVulkanImage(pixels);
    QTemporaryDir directory;
    const auto path=directory.filePath("mask.png");
    QImage mask=PointEffectPixels();
    REQUIRE(mask.save(path));
    openshot::Brightness brightness,driven_brightness;
    brightness.brightness=driven_brightness.brightness=openshot::Keyframe(0.3);
    driven_brightness.mask_mode=openshot::BRIGHTNESS_MASK_VARY_STRENGTH;
    openshot::Hue hue,driven_hue;
    hue.hue=driven_hue.hue=openshot::Keyframe(0.3);
    driven_hue.mask_mode=openshot::HUE_MASK_VARY_STRENGTH;
    openshot::Negate negate;
    openshot::Bars bars;
    openshot::Shift shift;
    shift.x=openshot::Keyframe(0.25);
    openshot::ColorShift channels;
    channels.alpha_x=openshot::Keyframe(-0.4);
    channels.red_y=openshot::Keyframe(0.2);
    for(openshot::EffectBase* effect : std::vector<openshot::EffectBase*>{
        &brightness,&driven_brightness,&hue,&driven_hue,&negate,&bars,&shift,&channels}) {
        effect->MaskReader(new openshot::QtImageReader(path.toStdString()));
        for(bool invert : {false,true}) {
            effect->mask_invert=invert;
            INFO(effect->info.class_name << " invert=" << invert);
            CheckVulkanEffect(*effect,pixels,source,1,2);
            // CPU comparison accesses mutable mask bits, invalidating the
            // upload cache key. Check reuse without another CPU mask access.
            const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            for(int number : {2,3,4}) {
                auto frame=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
                frame->AddDeferredImage(source);
                openshot::gpu_metrics::Scope scope(false);
                effect->ProcessFrame(frame,number);
                REQUIRE(frame->GetDeferredImage());
            }
            const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(after["uploads"]==before["uploads"]);
            CHECK(after["readbacks"]==before["readbacks"]);
        }
    }
}

TEST_CASE("Vulkan point effect chain stays resident and matches CPU", "[vulkan][hardware][effects][point-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels();
    auto cpu=std::make_shared<openshot::Frame>(1,pixels.width(),pixels.height(),"transparent");
    auto gpu=std::make_shared<openshot::Frame>(1,pixels.width(),pixels.height(),"transparent");
    cpu->AddImage(std::make_shared<QImage>(pixels.copy()));
    gpu->AddDeferredImage(openshot::UploadVulkanImage(pixels));
    openshot::Brightness brightness;
    brightness.brightness=openshot::Keyframe(0.18);
    openshot::Hue hue(openshot::Keyframe(-0.37));
    openshot::Negate negate;
    openshot::Bars bars;
    openshot::Shift shift;
    shift.x=openshot::Keyframe(-1.27);
    shift.y=openshot::Keyframe(0.5);
    openshot::ColorShift channels;
    channels.red_y=openshot::Keyframe(-0.31);
    channels.alpha_x=openshot::Keyframe(0.21);
    std::vector<openshot::EffectBase*> effects{&brightness,&hue,&negate,&bars,&shift,&channels};
    openshot::ResetGpuMetrics();
    for(auto effect : effects) {
        ProcessCpuEffect(*effect,cpu,1);
        openshot::gpu_metrics::Scope scope(false);
        effect->ProcessFrame(gpu,1);
        REQUIRE(gpu->GetDeferredImage());
    }
    const auto metrics=openshot::stringToJson(openshot::GpuMetricsSnapshot());
    CHECK(metrics["uploads"].asInt()==0);
    CHECK(metrics["readbacks"].asInt()==0);
    const auto expected=cpu->GetImage(),actual=gpu->GetImage();
    int max_error=0;
    for(int y=0;y<pixels.height();++y)
        for(int x=0;x<pixels.width()*4;++x)
            max_error=std::max(max_error,std::abs(int(expected->constScanLine(y)[x])-int(actual->constScanLine(y)[x])));
    INFO("six-effect chain max byte error=" << max_error);
    CHECK(max_error<=3);
}

namespace {
QImage SilhouettePixels(int w=65,int h=49) {
    QImage image(w,h,QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        if((x-w/2)*(x-w/2)+(y-h/2)*(y-h/2)<(h/3)*(h/3) ||
            (x>w/6 && x<w/3 && y>h/6 && y<h/2))
            image.setPixelColor(x,y,QColor((x*17)%256,(y*23)%256,(x*11+y*7)%256,((x+y)%4)*64+63));
    }
    return image;
}
}

TEST_CASE("Vulkan chroma key matches CPU hard and soft thresholds", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    QImage pixels=PointEffectPixels(47,19);
    for(int x=0;x<pixels.width();++x) pixels.setPixelColor(x,0,QColor(x*5,255-x*4,x*3));
    auto source=openshot::UploadVulkanImage(pixels);
    for(auto method : {openshot::CHROMAKEY_BASIC,openshot::CHROMAKEY_BASIC_SOFT}) {
        for(int fuzz : {0,1,20,100,300}) for(int halo : {0,1,40}) {
            INFO("method=" << method << " fuzz=" << fuzz << " halo=" << halo);
            openshot::ChromaKey effect(openshot::Color("#14ef0c"),openshot::Keyframe(fuzz),openshot::Keyframe(halo),method);
            CheckVulkanEffect(effect,pixels,source,1,1,true);
        }
    }
}

TEST_CASE("Vulkan pixelate matches Qt scaling regions alpha and animation", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    for(QSize size : {QSize(37,23),QSize(1,1),QSize(1,29),QSize(29,1)}) {
        auto pixels=PointEffectPixels(size.width(),size.height());
        auto source=openshot::UploadVulkanImage(pixels);
        openshot::Pixelate effect;
        for(double margin : {0.0,0.17,0.6}) {
            effect.left=effect.top=openshot::Keyframe(margin);
            effect.bottom=openshot::Keyframe(margin/2);
            for(double amount : {0.0,0.001,0.05,0.2,0.5,1.0}) {
                INFO("size=" << size.width() << "x" << size.height() << " margin=" << margin << " amount=" << amount);
                effect.pixelization=openshot::Keyframe(amount);
                CheckVulkanEffect(effect,pixels,source,1,1,true);
            }
        }
    }
}

TEST_CASE("Vulkan sharpen matches CPU channels modes adaptive threshold and alpha", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    auto pixels=PointEffectPixels(65,49);
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::Sharpen effect;
    effect.amount=openshot::Keyframe(2.0);
    for(int mode : {0,1}) for(int channel : {0,1,2}) {
        effect.mode=mode;effect.channel=channel;
        for(double radius : {0.0,3.0,40.0,100.0}) for(double threshold : {0.0,0.3,1.0}) {
            INFO("mode=" << mode << " channel=" << channel << " radius=" << radius << " threshold=" << threshold);
            effect.radius=openshot::Keyframe(radius);effect.threshold=openshot::Keyframe(threshold);
            CheckVulkanEffect(effect,pixels,source,1,1,true);
        }
    }
}

TEST_CASE("Vulkan shadow and glow match CPU spread blur offsets and inner mode", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    for(QSize size : {QSize(65,49),QSize(1,1),QSize(3,7)}) {
        auto pixels=SilhouettePixels(size.width(),size.height());
        if(size.width()==1) pixels.setPixelColor(0,0,QColor(130,80,230,127));
        auto source=openshot::UploadVulkanImage(pixels);
        openshot::Shadow shadow;
        openshot::Glow glow;
        shadow.color=glow.color=openshot::Color("#c43f85");
        shadow.color.alpha=glow.color.alpha=openshot::Keyframe(180);
        shadow.angle=openshot::Keyframe(-33);
        shadow.distance=openshot::Keyframe(-5);
        shadow.opacity=glow.opacity=openshot::Keyframe(0.63);
        for(int radius : {0,1,8,30}) for(double spread : {0.0,0.45}) {
            INFO("size=" << size.width() << "x" << size.height() << " radius=" << radius << " spread=" << spread);
            shadow.blur_radius=glow.blur_radius=openshot::Keyframe(radius);
            shadow.spread=glow.spread=openshot::Keyframe(spread);
            CheckVulkanEffect(shadow,pixels,source,1,1,true);
            for(int mode : {openshot::GLOW_MODE_OUTER,openshot::GLOW_MODE_INNER}) {
                glow.mode=mode;INFO("glow mode=" << mode);
                CheckVulkanEffect(glow,pixels,source,1,1,true);
            }
        }
    }
}

#ifdef USE_OPENCV
TEST_CASE("Vulkan outline matches OpenCV edges and premultiplied alpha", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    for(QImage pixels : {SilhouettePixels(),PointEffectPixels(),SilhouettePixels(1,5)}) {
        auto source=openshot::UploadVulkanImage(pixels);
        openshot::Outline effect;
        effect.color=openshot::Color("#c43f85");
        effect.color.alpha=openshot::Keyframe(170);
        for(double width : {0.0,0.5,1.0,3.0,7.5,20.0,100.0}) {
            INFO("width=" << width << " size=" << pixels.width() << "x" << pixels.height());
            effect.width=openshot::Keyframe(width);
            CheckVulkanEffect(effect,pixels,source,1,1,true);
        }
    }
}

#endif // USE_OPENCV

TEST_CASE("Vulkan advanced chroma key matches Babl color spaces", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(47,19);
    const auto source=openshot::UploadVulkanImage(pixels);
    for(int method=1;method<=10;++method) for(int fuzz : {0,1,20,60,120}) for(int halo : {0,1,25}) {
        INFO("method=" << method << " fuzz=" << fuzz << " halo=" << halo);
        openshot::ChromaKey effect(openshot::Color("#14ef0c"),openshot::Keyframe(fuzz),openshot::Keyframe(halo),openshot::ChromaKeyMethod(method));
        CheckVulkanEffect(effect,pixels,source,1,1,true);
    }
}

TEST_CASE("Vulkan spatial effects match animated CPU masks and reuse mask uploads", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=SilhouettePixels();
    const auto source=openshot::UploadVulkanImage(pixels);
    QTemporaryDir directory;
    const auto mask_path=directory.filePath("spatial-mask.png");
    REQUIRE(PointEffectPixels(23,17).save(mask_path));
    openshot::ChromaKey chroma(openshot::Color("#14ef0c"),openshot::Keyframe(30),openshot::Keyframe(20));
    openshot::Pixelate pixelate,driven_pixelate;
    pixelate.pixelization.AddPoint(1,0.1,openshot::LINEAR);pixelate.pixelization.AddPoint(9,0.8,openshot::LINEAR);
    driven_pixelate.pixelization=pixelate.pixelization;
    driven_pixelate.mask_mode=openshot::PIXELATE_MASK_VARY_STRENGTH;
    openshot::Sharpen sharpen,driven_sharpen;
    sharpen.amount.AddPoint(1,0.5,openshot::LINEAR);sharpen.amount.AddPoint(9,2.0,openshot::LINEAR);
    sharpen.radius=openshot::Keyframe(40);sharpen.threshold=openshot::Keyframe(0.3);
    driven_sharpen.amount=sharpen.amount;driven_sharpen.radius=sharpen.radius;driven_sharpen.threshold=sharpen.threshold;
    driven_sharpen.mask_mode=openshot::SHARPEN_MASK_VARY_STRENGTH;
    openshot::Shadow shadow;
    openshot::Glow glow;
    shadow.blur_radius.AddPoint(1,0,openshot::LINEAR);shadow.blur_radius.AddPoint(9,12,openshot::LINEAR);
    shadow.distance.AddPoint(1,-4,openshot::LINEAR);shadow.distance.AddPoint(9,8,openshot::LINEAR);
    glow.blur_radius=shadow.blur_radius;
    std::vector<openshot::EffectBase*> effects{&chroma,&pixelate,&driven_pixelate,&sharpen,&driven_sharpen,&shadow,&glow};
#ifdef USE_OPENCV
    openshot::Outline outline;
    outline.width.AddPoint(1,0.5,openshot::LINEAR);outline.width.AddPoint(9,10,openshot::LINEAR);
    effects.push_back(&outline);
#endif
    for(auto* effect:effects) {
        effect->MaskReader(new openshot::QtImageReader(mask_path.toStdString()));
        for(bool invert:{false,true}) {
            INFO(effect->info.class_name << " invert=" << invert);
            effect->mask_invert=invert;
            for(int number:{1,5,9,3}) CheckVulkanEffect(*effect,pixels,source,number,2);
            const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            for(int number:{4,6,8}) {
                auto frame=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
                frame->AddDeferredImage(source);
                openshot::gpu_metrics::Scope scope(false);
                effect->ProcessFrame(frame,number);
                REQUIRE(frame->GetDeferredImage());
            }
            const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(after["uploads"]==before["uploads"]);
            CHECK(after["readbacks"]==before["readbacks"]);
        }
    }
}

#ifdef USE_OPENCV
TEST_CASE("Vulkan spatial effect timeline chain stays copy free", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=SilhouettePixels(128,96);
    const auto source=openshot::UploadVulkanImage(pixels);
    std::vector<QImage> expected;
    for(bool gpu:{false,true}) {
        struct Restore {
            std::string backend,device;
            ~Restore(){openshot::ConfigureGpuEnvironment(backend,device);}
        } restore{std::getenv("OPENSHOT_RENDER_BACKEND")?std::getenv("OPENSHOT_RENDER_BACKEND"):"",
                  std::getenv("OPENSHOT_VULKAN_DEVICE")?std::getenv("OPENSHOT_VULKAN_DEVICE"):""};
        openshot::ConfigureGpuEnvironment(gpu?"vulkan":"cpu",restore.device);
        PatternReader reader(source,gpu);
        openshot::Clip clip(&reader);
        openshot::ChromaKey chroma;
        openshot::Pixelate pixelate;
        pixelate.pixelization=openshot::Keyframe(0.2);
        openshot::Outline outline;
        outline.width=openshot::Keyframe(3);
        openshot::Sharpen sharpen;
        sharpen.amount=openshot::Keyframe(1.3);sharpen.radius=openshot::Keyframe(20);
        openshot::Shadow shadow;
        shadow.blur_radius=openshot::Keyframe(5);shadow.distance=openshot::Keyframe(4);
        openshot::Glow glow;
        glow.blur_radius=openshot::Keyframe(4);
        int order=10;
        for(auto* effect:std::vector<openshot::EffectBase*>{&chroma,&pixelate,&outline,&sharpen,&shadow,&glow}) {
            effect->Order(order--);clip.AddEffect(effect);
        }
        openshot::Timeline timeline(128,96,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
        timeline.AddClip(&clip);timeline.Open();
        openshot::ResetGpuMetrics();
        std::vector<std::shared_ptr<openshot::Frame>> frames;
        for(int number=1;number<=8;++number) {
            frames.push_back(timeline.GetFrame(number));
            if(gpu) REQUIRE(frames.back()->GetDeferredImage());
        }
        if(gpu) {
            const auto metrics=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(metrics["readbacks"].asInt()==0);CHECK(metrics["uploads"].asInt()==0);
        }
        for(size_t number=0;number<frames.size();++number) {
            auto actual=frames[number]->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            if(!gpu) { expected.push_back(actual);continue; }
            int max_error=0;
            for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x)
                max_error=std::max(max_error,std::abs(int(actual.constScanLine(y)[x])-int(expected[number].constScanLine(y)[x])));
            INFO("timeline frame=" << number+1 << " max error=" << max_error);
            CHECK(max_error<=3);
        }
        timeline.Close();
    }
    const QImage unchanged=source->Materialize();
    CHECK(unchanged==pixels);
}
#endif

TEST_CASE("Vulkan chroma key preserves exact colors at hard color space boundaries", "[vulkan][hardware][effects][spatial-effects]") {
    if(!VulkanAvailable()) return;
    QImage pixels(16,3,QImage::Format_RGBA8888_Premultiplied);
    const std::array<QColor,8> palette{Qt::black,Qt::white,Qt::red,Qt::green,Qt::blue,Qt::cyan,Qt::magenta,QColor(128,128,128)};
    for(int y=0;y<pixels.height();++y) for(int x=0;x<pixels.width();++x) {
        QColor color=palette[x%palette.size()];color.setAlpha(y==0?255:y==1?127:1);pixels.setPixelColor(x,y,color);
    }
    const auto source=openshot::UploadVulkanImage(pixels);
    for(const auto& key:palette) for(int method=0;method<=11;++method) for(int fuzz:{0,1,125}) {
        INFO("key=" << key.name().toStdString() << " method=" << method << " fuzz=" << fuzz);
        openshot::ChromaKey effect(openshot::Color(key),openshot::Keyframe(fuzz),openshot::Keyframe(0),openshot::ChromaKeyMethod(method));
        CheckVulkanEffect(effect,pixels,source,1,1,true);
    }
}

TEST_CASE("Vulkan wave and deinterlace preserve CPU sampling", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size:{QSize(1,1),QSize(2,3),QSize(37,23),QSize(257,129)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());
        const auto source=openshot::UploadVulkanImage(pixels);
        openshot::Wave wave;
        wave.amplitude=openshot::Keyframe(2.5);wave.shift_x=openshot::Keyframe(-.8);
        for(int n:{1,13,87}) CheckVulkanEffect(wave,pixels,source,n,0,true);
        for(bool odd:{false,true}) {
            openshot::Deinterlace deinterlace(odd);
            CheckVulkanEffect(deinterlace,pixels,source,1,0,true);
        }
    }
}
TEST_CASE("Vulkan film grain preserves deterministic colored temporal noise", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(129,73);
    const auto source=openshot::UploadVulkanImage(pixels);
    openshot::FilmGrain grain;grain.amount=openshot::Keyframe(.85);
    for(int mode=0;mode<4;++mode) {
        grain.softness=openshot::Keyframe(mode&1?.9:0);
        grain.clump=openshot::Keyframe(mode&2?.8:0);
        grain.color_amount=openshot::Keyframe(mode&1?1:0);
        grain.color_variation=openshot::Keyframe(.9);
        for(int n:{1,19,53}) CheckVulkanEffect(grain,pixels,source,n,1,true);
    }
}
TEST_CASE("Vulkan reader scaling matches Qt smooth sampling", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize input:{QSize(1,1),QSize(7,11),QSize(37,23)}) {
        const auto pixels=PointEffectPixels(input.width(),input.height());
        const auto source=openshot::UploadVulkanImage(pixels);
        for(const QSize output:{QSize(1,1),QSize(5,7),QSize(17,13),QSize(49,29)}) {
            auto expected=pixels.scaled(output,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
            auto actual=openshot::ScaleEffectVulkanImage(source,output.width(),output.height())->Materialize();
            int error=0;
            for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x)
                error=std::max(error,std::abs(int(actual.constScanLine(y)[x])-int(expected.constScanLine(y)[x])));
            INFO("input="<<input.width()<<"x"<<input.height()<<" output="<<output.width()<<"x"<<output.height()<<" error="<<error);
            CHECK(error==0);
        }
    }
}
TEST_CASE("Vulkan denoise matches CPU spatial and temporal histories", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    for(int mode=0;mode<3;++mode) {
        openshot::DenoiseImage cpu,gpu;
        for(auto* effect:{&cpu,&gpu}) {
            effect->strength=openshot::Keyframe(mode==0?.78:1);
            effect->detail=openshot::Keyframe(mode==0?.5:mode==1?.1:.4);
            effect->temporal=openshot::Keyframe(mode==1?0:1);
        }
        for(int n:{1,2,3,19,20,19,20,21}) {
            auto pixels=PointEffectPixels(n<20?73:39,n<20?41:27);
            for(int y=0;y<pixels.height();++y) for(int x=0;x<pixels.width();++x) {
                auto* c=pixels.scanLine(y)+4*x;c[0]=std::min(int(c[3]),int(c[0])+n%3);
            }
            if(n==3) pixels.fill(Qt::white);
            auto source=openshot::UploadVulkanImage(pixels);
            auto expected=std::make_shared<openshot::Frame>(n,pixels.width(),pixels.height(),"transparent");
            expected->AddImage(std::make_shared<QImage>(pixels));
            ProcessCpuEffect(cpu,expected,n);
            auto actual=std::make_shared<openshot::Frame>(n,pixels.width(),pixels.height(),"transparent");actual->AddDeferredImage(source);
            const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            { openshot::gpu_metrics::Scope scope(false);gpu.ProcessFrame(actual,n); }
            const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);
            REQUIRE(actual->GetDeferredImage());
            auto a=actual->GetImage(),e=expected->GetImage();int error=0;
            for(int y=0;y<pixels.height();++y) for(int x=0;x<pixels.width()*4;++x)
                error=std::max(error,std::abs(int(a->constScanLine(y)[x])-int(e->constScanLine(y)[x])));
            INFO("mode="<<mode<<" frame="<<n<<" error="<<error);CHECK(error<=1);
        }
    }
}
TEST_CASE("Vulkan tape preserves seeded tracking chroma and noise", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size:{QSize(1,1),QSize(3,5),QSize(129,73),QSize(513,257)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());
        const auto source=openshot::UploadVulkanImage(pixels);
        openshot::AnalogTape tape;
        for(int mode=0;mode<3;++mode) {
            tape.noise=openshot::Keyframe(mode==0?0:.85);
            tape.tracking=openshot::Keyframe(mode==1?0:.9);
            tape.staticBands=openshot::Keyframe(mode==2?1:0);
            for(int n:{1,2,7,31}) { INFO("size="<<size.width()<<" mode="<<mode);CheckVulkanEffect(tape,pixels,source,n,1,true); }
        }
    }
}
TEST_CASE("Vulkan displacement consumes GPU video maps without transfers", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(97,53),map_pixels=PointEffectPixels(31,19);
    const auto source=openshot::UploadVulkanImage(pixels),map=openshot::UploadVulkanImage(map_pixels);
    openshot::Displace effect;
    for(bool gpu_map:{false,true}) {
        effect.Reader(new PatternReader(map,gpu_map));
        effect.horizontal=openshot::Keyframe(.23);effect.vertical=openshot::Keyframe(-.17);
        effect.brightness=openshot::Keyframe(.12);effect.contrast=openshot::Keyframe(3);
        for(bool invert:{false,true}) for(bool replace:{false,true}) {
            effect.invert=invert;effect.replace_image=replace;
            // CPU oracle may materialize the GPU map; its upload is outside the measured effect.
            CheckVulkanEffect(effect,pixels,source,19,1,gpu_map);
        }
    }
}

TEST_CASE("Vulkan grain and distortion effects preserves masks and reuses their upload", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(79,43);const auto source=openshot::UploadVulkanImage(pixels);
    QTemporaryDir directory;auto path=directory.filePath("mask.png");
    REQUIRE(PointEffectPixels(23,17).save(path));
    openshot::Wave wave;openshot::Deinterlace deinterlace;openshot::FilmGrain grain;
    openshot::DenoiseImage denoise;denoise.temporal=openshot::Keyframe(0);
    openshot::AnalogTape tape;openshot::Displace displace;
    displace.Reader(new PatternReader(source,true));
    grain.amount.AddPoint(1,.3,openshot::LINEAR);grain.amount.AddPoint(9,.9,openshot::LINEAR);
    wave.amplitude.AddPoint(1,.1,openshot::LINEAR);wave.amplitude.AddPoint(9,2,openshot::LINEAR);
    for(auto* effect:std::vector<openshot::EffectBase*>{&wave,&deinterlace,&grain,&denoise,&tape,&displace}) {
        effect->MaskReader(new openshot::QtImageReader(path.toStdString()));
        for(bool invert:{false,true}) {
            effect->mask_invert=invert;INFO(effect->info.class_name<<" invert="<<invert);
            for(int n:{1,5,9,3}) CheckVulkanEffect(*effect,pixels,source,n,2);
            const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            for(int n:{4,6,8}) {
                auto frame=std::make_shared<openshot::Frame>(n,pixels.width(),pixels.height(),"transparent");frame->AddDeferredImage(source);
                { openshot::gpu_metrics::Scope scope(false);effect->ProcessFrame(frame,n); }
                REQUIRE(frame->GetDeferredImage());
            }
            const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);
        }
    }
}

TEST_CASE("Vulkan grain and distortion effects stays resident through a timeline", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(128,96);const auto source=openshot::UploadVulkanImage(pixels);
    std::vector<QImage> expected;
    for(bool gpu:{false,true}) {
        struct Restore {
            std::string backend,device;
            ~Restore(){openshot::ConfigureGpuEnvironment(backend,device);}
        } restore{std::getenv("OPENSHOT_RENDER_BACKEND")?std::getenv("OPENSHOT_RENDER_BACKEND"):"",
                  std::getenv("OPENSHOT_VULKAN_DEVICE")?std::getenv("OPENSHOT_VULKAN_DEVICE"):""};
        openshot::ConfigureGpuEnvironment(gpu?"vulkan":"cpu",restore.device);
        PatternReader reader(source,gpu);
        openshot::Clip clip(&reader);
        openshot::Wave wave;openshot::Deinterlace deinterlace;openshot::FilmGrain grain;
        openshot::DenoiseImage denoise;openshot::AnalogTape tape;openshot::Displace displace;
        grain.Id("grain-parity");tape.Id("tape-parity");
        displace.Reader(new PatternReader(source,gpu));
        int order=10;
        for(auto* effect:std::vector<openshot::EffectBase*>{&wave,&deinterlace,&grain,&denoise,&tape,&displace}) {
            effect->Order(order--);clip.AddEffect(effect);
        }
        openshot::Timeline timeline(192,144,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
        timeline.SetMaxSize(128,96);timeline.AddClip(&clip);timeline.Open();
        openshot::ResetGpuMetrics();
        std::vector<std::shared_ptr<openshot::Frame>> frames;
        for(int n=1;n<=8;++n) {
            frames.push_back(timeline.GetFrame(n));if(gpu) REQUIRE(frames.back()->GetDeferredImage());
        }
        if(gpu) {
            const auto metrics=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(metrics["uploads"].asInt()==0);CHECK(metrics["readbacks"].asInt()==0);
        }
        for(size_t n=0;n<frames.size();++n) {
            const auto actual=frames[n]->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            if(!gpu) { expected.push_back(actual);continue; }
            int error=0;double total=0;
            for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x) {
                int delta=std::abs(int(actual.constScanLine(y)[x])-int(expected[n].constScanLine(y)[x]));
                error=std::max(error,delta);total+=delta;
            }
            INFO("frame="<<n+1<<" max="<<error<<" mean="<<total/(actual.width()*actual.height()*4));
            CHECK(error<=4);CHECK(total/(actual.width()*actual.height()*4)<=.1);
        }
        timeline.Close();
    }
    CHECK(source->Materialize()==pixels);
}

TEST_CASE("Vulkan grain and distortion effects preserve precision at 4K and tiny boundaries", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size:{QSize(1,1),QSize(2,3),QSize(3840,2160)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());
        const auto source=openshot::UploadVulkanImage(pixels);
        openshot::Wave wave;wave.amplitude=openshot::Keyframe(2.3);wave.shift_x=openshot::Keyframe(-.35);
        openshot::Deinterlace deinterlace;
        openshot::FilmGrain grain;grain.amount=openshot::Keyframe(.9);grain.evolution=openshot::Keyframe(0);
        openshot::DenoiseImage denoise;denoise.temporal=openshot::Keyframe(0);
        denoise.strength=openshot::Keyframe(1);denoise.detail=openshot::Keyframe(.1);
        denoise.response_curve.enabled=openshot::Keyframe(0);
        openshot::AnalogTape tape;
        openshot::Displace displace;displace.Reader(new PatternReader(source,true));
        for(auto* effect:std::vector<openshot::EffectBase*>{&wave,&deinterlace,&grain,&denoise,&tape,&displace}) {
            INFO(effect->info.class_name<<" size="<<size.width()<<"x"<<size.height());
            CheckVulkanEffect(*effect,pixels,source,17,effect==&wave || effect==&deinterlace?0:1,true);
        }
        CHECK(source->Materialize()==pixels);
    }
}

TEST_CASE("Vulkan tape survives CPU GPU switches with different image sizes", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    openshot::AnalogTape tape;
    for(const QSize size:{QSize(129,73),QSize(3,1),QSize(129,73),QSize(17,23),QSize(129,73)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());
        const auto source=openshot::UploadVulkanImage(pixels);
        CheckVulkanEffect(tape,pixels,source,1,1,true);
        // Leave only the GPU scratch dimensions changed, then reuse the last CPU size.
        auto tiny=std::make_shared<openshot::Frame>(2,1,1,"transparent");
        tiny->AddDeferredImage(openshot::UploadVulkanImage(PointEffectPixels(1,1)));
        tape.ProcessFrame(tiny,2);REQUIRE(tiny->GetDeferredImage());
        CheckVulkanEffect(tape,pixels,source,3,1,true);
    }
}

TEST_CASE("Vulkan denoise resets temporal history when switching backends", "[vulkan][hardware][effects][batch-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(59,37);
    const auto source=openshot::UploadVulkanImage(pixels);
    openshot::DenoiseImage effect;
    effect.temporal=openshot::Keyframe(1);
    // Each comparison switches CPU -> GPU; neither backend may consume history
    // belonging to the other backend or to a disabled interval.
    for(int n:{1,2,3}) CheckVulkanEffect(effect,pixels,source,n,1,true);
    effect.strength=openshot::Keyframe(0);CheckVulkanEffect(effect,pixels,source,4,0,true);
    effect.strength=openshot::Keyframe(.9);CheckVulkanEffect(effect,pixels,source,5,1,true);
}

TEST_CASE("Vulkan lens flare preserves CPU rings reflectors tint and alpha", "[vulkan][hardware][effects][flare-object-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size:{QSize(1,1),QSize(37,23),QSize(257,129)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());const auto source=openshot::UploadVulkanImage(pixels);
        openshot::LensFlare effect;
        effect.x.AddPoint(1,-.7,openshot::LINEAR);effect.x.AddPoint(9,.4,openshot::LINEAR);
        effect.y.AddPoint(1,-.3,openshot::LINEAR);effect.y.AddPoint(9,.8,openshot::LINEAR);
        for(double brightness:{0.,.25,.5,1.,1.7}) for(double scale:{0.,.4,1.8}) {
            effect.brightness=openshot::Keyframe(brightness);effect.size=openshot::Keyframe(scale);
            for(const QColor color:{QColor(Qt::white),QColor(235,73,18,91)}) {
                effect.color=openshot::Color(color);
                INFO("size="<<size.width()<<" intensity="<<brightness<<" scale="<<scale<<" color="<<color.name().toStdString());
                for(int n:{1,5,9}) CheckVulkanEffect(effect,pixels,source,n,2,true);
            }
        }
    }
}

#ifdef USE_OPENCV
namespace {
void WriteObjectMaskData(const QString& path,int width,int height,const std::vector<std::vector<uint32_t>>& frames) {
    auto varint=[](std::string& out,uint64_t v) {
        while(v>=128) { out.push_back(char((v&127)|128));v>>=7; }out.push_back(char(v));
    };
    auto message=[&](std::string& out,uint32_t tag,const std::string& value) {
        varint(out,tag);varint(out,value.size());out+=value;
    };
    std::string data;
    for(size_t n=0;n<frames.size();++n) {
        std::string mask,box,frame;
        varint(mask,8);varint(mask,width);varint(mask,16);varint(mask,height);
        for(auto count:frames[n]) { varint(mask,24);varint(mask,count); }
        // Width/height float fields, followed by the nested segmentation mask.
        for(uint32_t tag:{29u,37u}) { varint(box,tag);box.append("\0\0\x80\x3f",4); }
        message(box,66,mask);varint(frame,8);varint(frame,n+1);message(frame,18,box);message(data,10,frame);
    }
    QFile file(path);REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(data.data(),data.size())==qint64(data.size()));file.close();
}
}
TEST_CASE("Vulkan object masks decode scale and dilate like CPU", "[vulkan][hardware][effects][flare-object-effects]") {
    if(!VulkanAvailable()) return;
    QTemporaryDir directory;const auto path=directory.filePath("masks.pb");
    WriteObjectMaskData(path,7,5,{{0,8,4,6,1,8,8},{35},{0,35},{1,1,1,1,1,1,1,1,1,1,1,1},{0,0,0,12,23}});
    openshot::ObjectMask effect;REQUIRE(effect.LoadObjMaskData(path.toStdString()));
    effect.mask_color=openshot::Color("#d34c29");effect.stroke_color=openshot::Color("#208fe1");
    for(const QSize size:{QSize(1,1),QSize(3,2),QSize(7,5),QSize(37,23),QSize(129,73)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());const auto source=openshot::UploadVulkanImage(pixels);
        for(double width:{0.,1.,3.,10.}) for(double alpha:{0.,.47,1.}) {
            effect.stroke_width=openshot::Keyframe(width);effect.mask_alpha=openshot::Keyframe(alpha);
            effect.stroke_alpha=openshot::Keyframe(alpha);
            INFO("size="<<size.width()<<" stroke="<<width<<" alpha="<<alpha);
            for(int n:{1,2,3,4,5,19}) CheckVulkanEffect(effect,pixels,source,n,1,true);
        }
        effect.draw_mask=openshot::Keyframe(0);CheckVulkanEffect(effect,pixels,source,1,0,true);effect.draw_mask=openshot::Keyframe(1);
    }
}
#endif

TEST_CASE("Vulkan lens flare and object masks preserve effect masks", "[vulkan][hardware][effects][flare-object-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(129,73);const auto source=openshot::UploadVulkanImage(pixels);
    QTemporaryDir directory;const auto path=directory.filePath("mask.png");REQUIRE(PointEffectPixels(23,17).save(path));
    openshot::LensFlare flare;
    flare.brightness.AddPoint(1,.25,openshot::LINEAR);flare.brightness.AddPoint(9,1.6,openshot::LINEAR);
    flare.color=openshot::Color("#e85a27");
    std::vector<openshot::EffectBase*> effects{&flare};
#ifdef USE_OPENCV
    const auto data_path=directory.filePath("objects.pb");WriteObjectMaskData(data_path,7,5,std::vector<std::vector<uint32_t>>(9,{0,8,4,6,1,8,8}));
    openshot::ObjectMask object;REQUIRE(object.LoadObjMaskData(data_path.toStdString()));
    object.mask_alpha.AddPoint(1,.2,openshot::LINEAR);object.mask_alpha.AddPoint(9,.8,openshot::LINEAR);
    object.stroke_width.AddPoint(1,1,openshot::LINEAR);object.stroke_width.AddPoint(9,6,openshot::LINEAR);
    effects.push_back(&object);
#endif
    for(auto* effect:effects) {
        effect->MaskReader(new openshot::QtImageReader(path.toStdString()));
        for(bool invert:{false,true}) {
            effect->mask_invert=invert;INFO(effect->info.class_name<<" invert="<<invert);
            for(int n:{1,5,9,3}) CheckVulkanEffect(*effect,pixels,source,n,2);
            const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            for(int n:{4,6,8}) {
                auto frame=std::make_shared<openshot::Frame>(n,pixels.width(),pixels.height(),"transparent");frame->AddDeferredImage(source);
                { openshot::gpu_metrics::Scope scope(false);effect->ProcessFrame(frame,n); }
                REQUIRE(frame->GetDeferredImage());
            }
            const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);
        }
    }
}

#ifdef USE_OPENCV
TEST_CASE("Vulkan object mask sources stay copy free for linked effects", "[vulkan][hardware][effects][flare-object-effects]") {
    if(!VulkanAvailable()) return;
    QTemporaryDir directory;const auto path=directory.filePath("objects.pb");
    WriteObjectMaskData(path,7,5,{{0,8,4,6,1,8,8},{35},{0,35}});
    openshot::ObjectMask provider;provider.Id("gpu-segmentation");provider.draw_mask=openshot::Keyframe(0);
    REQUIRE(provider.LoadObjMaskData(path.toStdString()));
    openshot::Clip parent;parent.AddEffect(&provider);
    openshot::Brightness effect(openshot::Keyframe(.3),openshot::Keyframe(4));
    effect.MaskSourceId(provider.Id());parent.AddEffect(&effect);
    const auto pixels=PointEffectPixels(97,53);const auto source=openshot::UploadVulkanImage(pixels);
    for(bool invert:{false,true}) {
        effect.mask_invert=invert;
        for(int n:{1,2,3,19,1}) CheckVulkanEffect(effect,pixels,source,n,1,true);
    }
}
#endif

TEST_CASE("Vulkan flare and segmentation preserve 4K precision", "[vulkan][hardware][effects][flare-object-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(3840,2160);const auto source=openshot::UploadVulkanImage(pixels);
    openshot::LensFlare flare;flare.x=openshot::Keyframe(.3);flare.y=openshot::Keyframe(-.2);
    flare.brightness=openshot::Keyframe(.73);flare.size=openshot::Keyframe(1.7);flare.color=openshot::Color("#e254a7");
    CheckVulkanEffect(flare,pixels,source,5,2,true);
#ifdef USE_OPENCV
    QTemporaryDir directory;const auto path=directory.filePath("objects.pb");
    WriteObjectMaskData(path,7,5,{{0,8,4,6,1,8,8}});
    openshot::ObjectMask effect;REQUIRE(effect.LoadObjMaskData(path.toStdString()));
    effect.mask_alpha=openshot::Keyframe(.62);effect.stroke_width=openshot::Keyframe(11);effect.stroke_alpha=openshot::Keyframe(.41);
    CheckVulkanEffect(effect,pixels,source,1,1,true);
#endif
    CHECK(source->Materialize()==pixels);
}

#ifdef USE_OPENCV
TEST_CASE("Vulkan flare segmentation and linked grading stay resident in a timeline", "[vulkan][hardware][effects][flare-object-effects]") {
    if(!VulkanAvailable()) return;
    QTemporaryDir directory;const auto path=directory.filePath("objects.pb");
    WriteObjectMaskData(path,7,5,std::vector<std::vector<uint32_t>>(8,{0,8,4,6,1,8,8}));
    const auto pixels=PointEffectPixels(128,96);const auto source=openshot::UploadVulkanImage(pixels);
    std::vector<QImage> expected;
    for(bool gpu:{false,true}) {
        struct Restore {
            std::string backend,device;
            ~Restore(){openshot::ConfigureGpuEnvironment(backend,device);}
        } restore{std::getenv("OPENSHOT_RENDER_BACKEND")?std::getenv("OPENSHOT_RENDER_BACKEND"):"",
                  std::getenv("OPENSHOT_VULKAN_DEVICE")?std::getenv("OPENSHOT_VULKAN_DEVICE"):""};
        openshot::ConfigureGpuEnvironment(gpu?"vulkan":"cpu",restore.device);
        PatternReader reader(source,gpu);openshot::Clip clip(&reader);
        openshot::LensFlare flare;flare.color=openshot::Color("#e86a23");
        flare.brightness.AddPoint(1,.2,openshot::LINEAR);flare.brightness.AddPoint(8,1.3,openshot::LINEAR);
        openshot::ObjectMask object;object.Id("timeline-object");REQUIRE(object.LoadObjMaskData(path.toStdString()));
        object.stroke_width=openshot::Keyframe(5);
        openshot::Brightness grade(openshot::Keyframe(.12),openshot::Keyframe(3));grade.MaskSourceId(object.Id());
        int order=10;
        for(auto* effect:std::vector<openshot::EffectBase*>{&flare,&object,&grade}) { effect->Order(order--);clip.AddEffect(effect); }
        openshot::Timeline timeline(128,96,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
        timeline.AddClip(&clip);timeline.Open();openshot::ResetGpuMetrics();
        std::vector<std::shared_ptr<openshot::Frame>> frames;
        for(int n=1;n<=8;++n) { frames.push_back(timeline.GetFrame(n));if(gpu) REQUIRE(frames.back()->GetDeferredImage()); }
        if(gpu) {
            const auto m=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            CHECK(m["uploads"].asInt()==0);CHECK(m["readbacks"].asInt()==0);
        }
        for(size_t n=0;n<frames.size();++n) {
            const auto actual=frames[n]->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            if(!gpu) { expected.push_back(actual);continue; }
            int error=0;double sum=0;
            for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x) {
                int d=std::abs(int(actual.constScanLine(y)[x])-int(expected[n].constScanLine(y)[x]));error=std::max(error,d);sum+=d;
            }
            INFO("frame="<<n+1<<" max="<<error<<" mean="<<sum/(actual.width()*actual.height()*4));
            CHECK(error<=3);CHECK(sum/(actual.width()*actual.height()*4)<=.1);
        }
        timeline.Close();
    }
    CHECK(source->Materialize()==pixels);
}
#endif

TEST_CASE("Vulkan spherical projection matches every model and sampler", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(37,23);const auto source=openshot::UploadVulkanImage(pixels);
    openshot::SphericalProjection effect;effect.yaw=openshot::Keyframe(23.7);effect.pitch=openshot::Keyframe(-14.1);effect.roll=openshot::Keyframe(7.4);
    effect.fov=openshot::Keyframe(113.7);effect.in_fov=openshot::Keyframe(213.2);
    for(int model=0;model<=4;++model) for(int projection=0;projection<=5;++projection) for(int invert:{0,1}) for(int sampler=0;sampler<=3;++sampler) {
        effect.input_model=model;effect.projection_mode=projection;effect.invert=invert;effect.interpolation=sampler;
        INFO("model="<<model<<" projection="<<projection<<" invert="<<invert<<" sampler="<<sampler);
        CheckVulkanEffect(effect,pixels,source,3,2,true);
    }
}
#ifdef USE_OPENCV
TEST_CASE("Vulkan stabilizer matches OpenCV fixed point warps", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    openshot::Stabilizer effect;
    effect.transformationData[1]=EffectTransformParam(.023,-.071,.127);
    effect.transformationData[2]=EffectTransformParam(-.21,.043,-.39);
    effect.transformationData[3]=EffectTransformParam(0,0,0);
    for(const QSize size:{QSize(1,1),QSize(3,5),QSize(37,23),QSize(257,129)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());const auto source=openshot::UploadVulkanImage(pixels);
        for(double zoom:{.7,1.,1.3,2.1}) {
            Json::Value values;values["zoom"]=openshot::Keyframe(zoom).JsonValue();effect.SetJsonValue(values);
            for(int n:{1,2,3,19}) { INFO("size="<<size.width()<<" zoom="<<zoom<<" frame="<<n);CheckVulkanEffect(effect,pixels,source,n,1,true); }
        }
    }
}
#endif
#ifdef USE_OPENCV
namespace {
void WriteDetectionData(const QString& path) {
    auto varint=[](std::string& out,uint64_t v){while(v>=128){out.push_back(char((v&127)|128));v>>=7;}out.push_back(char(v));};
    auto message=[&](std::string& out,int tag,const std::string& value){varint(out,tag);varint(out,value.size());out+=value;};
    auto real=[&](std::string& out,int tag,float value){varint(out,tag);uint32_t bits;std::memcpy(&bits,&value,4);for(int b=0;b<4;++b)out.push_back(char(bits>>(8*b)));};
    std::string data;message(data,26,"person");message(data,26,"car");
    for(int n=1;n<=9;++n) {
        std::string frame;varint(frame,8);varint(frame,n);
        for(int object=0;object<2;++object) {
            std::string box,mask;real(box,13,.13f+.005f*n+object*.16f);real(box,21,.21f+.007f*n+object*.1f);
            real(box,29,.3f+.003f*n);real(box,37,.37f);real(box,53,object?.4f:.95f);
            varint(box,40);varint(box,object);varint(box,56);varint(box,100+object);
            varint(mask,8);varint(mask,7);varint(mask,16);varint(mask,5);
            for(int count:{0,8,4,6,1,8,8}) {varint(mask,24);varint(mask,count);}message(box,66,mask);message(frame,18,box);
        }
        message(data,10,frame);
    }
    QFile file(path);REQUIRE(file.open(QIODevice::WriteOnly));REQUIRE(file.write(data.data(),data.size())==qint64(data.size()));file.close();
}
}
TEST_CASE("Vulkan detection boxes and segmentation match Qt", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    EnsureVulkanTestApplication();QTemporaryDir directory;auto path=directory.filePath("detections.pb");WriteDetectionData(path);
    openshot::Clip parent;openshot::ObjectDetection effect;parent.AddEffect(&effect);REQUIRE(effect.LoadObjDetectdData(path.toStdString()));
    for(const QSize size:{QSize(37,23),QSize(257,129)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());const auto source=openshot::UploadVulkanImage(pixels);
        for(int mode=0;mode<4;++mode) {
            Json::Value settings;settings["display_box_text"]=openshot::Keyframe(mode==3?1:0).JsonValue();effect.SetJsonValue(settings);
            for(auto& item:effect.trackedObjects) {
                auto tracked=std::static_pointer_cast<openshot::TrackedObjectBBox>(item.second);
                tracked->background_corner=openshot::Keyframe(mode==1?5:0);
                tracked->draw_mask=openshot::Keyframe(mode==2?1:0);
                tracked->draw_box=openshot::Keyframe(mode==2 || mode==3?0:1);
            }
            for(int n:{1,3,9}) { INFO("size="<<size.width()<<" mode="<<mode<<" frame="<<n);CheckVulkanEffect(effect,pixels,source,n,3,!(mode==3 && n==1 && size.width()==37)); }
        }
    }
}
#endif
TEST_CASE("Vulkan vector annotation coverage follows Qt paths", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    QImage pixels(97,73,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);const auto source=openshot::UploadVulkanImage(pixels);
    for(int mode=0;mode<2;++mode) {
        QPainterPath path;path.addRoundedRect(QRectF(12.31,17.43,54.27,36.29),5,5);
        if(mode) { QPainterPathStroker stroker;stroker.setWidth(2);path=stroker.createStroke(path); }
        const QColor color(83,160,237,255);QImage expected=pixels.copy();
        QPainter painter(&expected);painter.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);painter.fillPath(path,color);painter.end();
        auto actual=openshot::PaintVulkanPath(source,path,color)->Materialize();
        int error=0,ex=0,ey=0,channel=0;
        for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x) {
            int d=std::abs(int(actual.constScanLine(y)[x])-int(expected.constScanLine(y)[x]));if(d>error){error=d;ex=x/4;ey=y;channel=x%4;}
        }
        INFO("mode="<<mode<<" error="<<error<<" location="<<ex<<","<<ey<<" channel="<<channel<<" CPU="<<int(expected.constScanLine(ey)[4*ex+channel])<<" GPU="<<int(actual.constScanLine(ey)[4*ex+channel]));
        CHECK(error<=2);
    }
}
TEST_CASE("Vulkan spherical auto quality covers mipmaps and camera poles", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size:{QSize(1,1),QSize(3,5),QSize(129,73)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());const auto source=openshot::UploadVulkanImage(pixels);
        openshot::SphericalProjection effect;
        effect.yaw=openshot::Keyframe(19.7);effect.roll=openshot::Keyframe(12.3);
        effect.fov=openshot::Keyframe(160);effect.in_fov=openshot::Keyframe(70);
        for(int model=0;model<=4;++model) for(int projection:{0,1,2,4,5}) {
            effect.input_model=model;effect.projection_mode=projection;
            for(double pitch:{-90.,0.,90.}) {
                effect.pitch=openshot::Keyframe(pitch);
                INFO("size="<<size.width()<<" model="<<model<<" projection="<<projection<<" pitch="<<pitch);
                CheckVulkanEffect(effect,pixels,source,1,2,true);
            }
        }
    }
}
#ifdef USE_OPENCV
TEST_CASE("Vulkan detection linked masks honor filtering and visibility", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    EnsureVulkanTestApplication();QTemporaryDir directory;auto path=directory.filePath("detections.pb");WriteDetectionData(path);
    openshot::Clip parent;openshot::ObjectDetection provider;provider.Id("detection-provider");parent.AddEffect(&provider);
    REQUIRE(provider.LoadObjDetectdData(path.toStdString()));
    openshot::Brightness effect(openshot::Keyframe(.3),openshot::Keyframe(2));effect.MaskSourceId(provider.Id());parent.AddEffect(&effect);
    const auto pixels=PointEffectPixels(129,73);const auto source=openshot::UploadVulkanImage(pixels);
    for(int mode=0;mode<4;++mode) {
        Json::Value settings;settings["class_filter"]=mode==1?"person":"";settings["confidence_threshold"]=mode==2?.9:.25;
        provider.SetJsonValue(settings);
        for(auto& item:provider.trackedObjects) {
            auto object=std::static_pointer_cast<openshot::TrackedObjectBBox>(item.second);
            object->draw_mask=openshot::Keyframe(mode==3?1:0);object->background_corner=openshot::Keyframe(5);
        }
        for(bool invert:{false,true}) {
            effect.mask_invert=invert;
            for(int n:{1,3,9,19}) { INFO("mode="<<mode<<" invert="<<invert<<" frame="<<n);CheckVulkanEffect(effect,pixels,source,n,2,true); }
        }
    }
}
#endif

TEST_CASE("Vulkan projection and stabilization preserve 4K sampling", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    // Smooth color detail still spans alpha extremes while tolerating subpixel
    // differences in the CPU double and GPU float camera calculations.
    QImage pixels(3840,2160,QImage::Format_RGBA8888_Premultiplied);
    for(int y=0;y<pixels.height();++y) for(int x=0;x<pixels.width();++x)
        pixels.setPixelColor(x,y,QColor((x/19+y/17)%256,(x/23+y/31)%256,(x/37+y/11)%256,255));
    const auto source=openshot::UploadVulkanImage(pixels);
    openshot::SphericalProjection sphere;sphere.yaw=openshot::Keyframe(21.7);sphere.pitch=openshot::Keyframe(-23.1);sphere.roll=openshot::Keyframe(7.3);
    CheckVulkanEffect(sphere,pixels,source,1,2,true);
#ifdef USE_OPENCV
    openshot::Stabilizer stabilizer;stabilizer.transformationData[1]=EffectTransformParam(.034,-.021,.043);
    CheckVulkanEffect(stabilizer,pixels,source,1,1,true);
    EnsureVulkanTestApplication();QTemporaryDir directory;auto path=directory.filePath("detection.pb");WriteDetectionData(path);
    openshot::Clip parent;openshot::ObjectDetection detector;parent.AddEffect(&detector);REQUIRE(detector.LoadObjDetectdData(path.toStdString()));
    // Warm the glyph assets. Subsequent moving labels, boxes and masks upload no images.
    CheckVulkanEffect(detector,pixels,source,1,3);
    CheckVulkanEffect(detector,pixels,source,3,3,true);
#endif
    CHECK(source->Materialize()==pixels);
}
#ifdef USE_OPENCV
TEST_CASE("Vulkan projection stabilization and detection preserve shared masks", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    EnsureVulkanTestApplication();QTemporaryDir directory;auto path=directory.filePath("detection.pb");WriteDetectionData(path);
    auto mask_path=directory.filePath("mask.png");REQUIRE(PointEffectPixels(23,17).save(mask_path));
    const auto pixels=PointEffectPixels(129,73);const auto source=openshot::UploadVulkanImage(pixels);
    openshot::SphericalProjection sphere;sphere.yaw.AddPoint(1,-17,openshot::LINEAR);sphere.yaw.AddPoint(9,23,openshot::LINEAR);
    openshot::Stabilizer stabilize;for(int n=1;n<=9;++n) stabilize.transformationData[n]=EffectTransformParam(.003*n,-.002*n,.007*n);
    openshot::ObjectDetection detect;openshot::Clip parent;parent.AddEffect(&detect);REQUIRE(detect.LoadObjDetectdData(path.toStdString()));
    for(auto* effect:std::vector<openshot::EffectBase*>{&sphere,&stabilize,&detect}) {
        effect->MaskReader(new openshot::QtImageReader(mask_path.toStdString()));
        for(bool invert:{false,true}) {
            effect->mask_invert=invert;INFO(effect->info.class_name<<" invert="<<invert);
            for(int n:{1,3,9}) CheckVulkanEffect(*effect,pixels,source,n,3);
            auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            for(int n:{2,4,8}) {
                auto frame=std::make_shared<openshot::Frame>(n,129,73,"transparent");frame->AddDeferredImage(source);
                { openshot::gpu_metrics::Scope scope(false);effect->ProcessFrame(frame,n); }REQUIRE(frame->GetDeferredImage());
            }
            auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);
        }
    }
}
TEST_CASE("Vulkan animated projection detection chain remains copy free", "[vulkan][hardware][effects][projection-detection-effects]") {
    if(!VulkanAvailable()) return;
    EnsureVulkanTestApplication();QTemporaryDir directory;auto path=directory.filePath("detection.pb");WriteDetectionData(path);
    const auto pixels=PointEffectPixels(128,96);const auto source=openshot::UploadVulkanImage(pixels);std::vector<QImage> expected;
    for(bool gpu:{false,true}) {
        struct Restore { std::string backend,device;~Restore(){openshot::ConfigureGpuEnvironment(backend,device);} }
        restore{std::getenv("OPENSHOT_RENDER_BACKEND")?std::getenv("OPENSHOT_RENDER_BACKEND"):"",std::getenv("OPENSHOT_VULKAN_DEVICE")?std::getenv("OPENSHOT_VULKAN_DEVICE"):""};
        openshot::ConfigureGpuEnvironment(gpu?"vulkan":"cpu",restore.device);
        PatternReader reader(source,gpu);openshot::Clip clip(&reader);
        openshot::SphericalProjection sphere;sphere.yaw.AddPoint(1,-12.3,openshot::LINEAR);sphere.yaw.AddPoint(8,21.7,openshot::LINEAR);
        openshot::Stabilizer stabilize;for(int n=1;n<=9;++n) stabilize.transformationData[n]=EffectTransformParam(.001*n,-.002*n,.003*n);
        openshot::ObjectDetection detect;detect.Id("chain-detections");
        openshot::Brightness brightness(openshot::Keyframe(.12),openshot::Keyframe(2));brightness.MaskSourceId(detect.Id());
        int order=10;for(auto* effect:std::vector<openshot::EffectBase*>{&sphere,&stabilize,&detect,&brightness}) { effect->Order(order--);clip.AddEffect(effect); }
        REQUIRE(detect.LoadObjDetectdData(path.toStdString()));
        openshot::Timeline timeline(128,96,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);timeline.AddClip(&clip);timeline.Open();
        std::vector<std::shared_ptr<openshot::Frame>> frames{timeline.GetFrame(1)};openshot::ResetGpuMetrics();
        for(int n=2;n<=8;++n) { frames.push_back(timeline.GetFrame(n));if(gpu) REQUIRE(frames.back()->GetDeferredImage()); }
        if(gpu) { auto m=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(m["uploads"].asInt()==0);CHECK(m["readbacks"].asInt()==0); }
        for(size_t n=0;n<frames.size();++n) {
            auto actual=frames[n]->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);if(!gpu) { expected.push_back(actual);continue; }
            int error=0;double sum=0;for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x) { int d=std::abs(int(actual.constScanLine(y)[x])-int(expected[n].constScanLine(y)[x]));error=std::max(error,d);sum+=d; }
            INFO("frame="<<n+1<<" max="<<error<<" mean="<<sum/(actual.width()*actual.height()*4));CHECK(error<=5);CHECK(sum/(actual.width()*actual.height()*4)<=.1);
        }
        timeline.Close();
    }
    CHECK(source->Materialize()==pixels);
}
#endif
#ifdef USE_OPENCV
namespace {
void TrackerTestBoxes(openshot::Tracker& tracker) {
    tracker.trackedData->SetBaseFPS(openshot::Fraction(24,1));
    tracker.trackedData->AddBox(1,.27f,.39f,.43f,.47f,0);
    tracker.trackedData->AddBox(9,.71f,.61f,.63f,.32f,37);
    tracker.trackedData->stroke=openshot::Color("#c34b27");
    tracker.trackedData->background=openshot::Color("#2691da");
}
}
TEST_CASE("Vulkan tracker preserves animated CPU boxes and styles", "[vulkan][hardware][effects][tracker-effects]") {
    if(!VulkanAvailable()) return;
    openshot::Tracker effect;TrackerTestBoxes(effect);
    for(const QSize size:{QSize(1,1),QSize(3,5),QSize(37,23),QSize(257,129)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());const auto source=openshot::UploadVulkanImage(pixels);
        for(double corner:{0.,3.,12.}) for(double width:{0.,1.,7.2}) for(double alpha:{0.,.45,1.}) {
            effect.trackedData->background_corner=openshot::Keyframe(corner);effect.trackedData->stroke_width=openshot::Keyframe(width);
            effect.trackedData->stroke_alpha=openshot::Keyframe(alpha);effect.trackedData->background_alpha=openshot::Keyframe(alpha);
            for(int n:{1,4,9,19}) {
                INFO("size="<<size.width()<<"x"<<size.height()<<" corner="<<corner<<" stroke="<<width<<" alpha="<<alpha<<" frame="<<n);
                CheckVulkanEffect(effect,pixels,source,n,3,true);
            }
        }
    }
}
TEST_CASE("Vulkan tracker missing and hidden boxes stay resident", "[vulkan][hardware][effects][tracker-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels();const auto source=openshot::UploadVulkanImage(pixels);
    openshot::Tracker effect;CheckVulkanEffect(effect,pixels,source,1,0,true);
    TrackerTestBoxes(effect);effect.trackedData->visible=openshot::Keyframe(0);CheckVulkanEffect(effect,pixels,source,3,0,true);
    effect.trackedData->visible=openshot::Keyframe(1);effect.trackedData->draw_box=openshot::Keyframe(0);
    effect.trackedData->draw_text=openshot::Keyframe(1);CheckVulkanEffect(effect,pixels,source,3,0,true);
    effect.trackedData.reset();CheckVulkanEffect(effect,pixels,source,1,0,true);
    CHECK(!effect.GetFrame(std::shared_ptr<openshot::Frame>(),1));
}
TEST_CASE("Vulkan tracker preserves shared masks and upload reuse", "[vulkan][hardware][effects][tracker-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(129,73);const auto source=openshot::UploadVulkanImage(pixels);
    QTemporaryDir directory;auto path=directory.filePath("mask.png");REQUIRE(PointEffectPixels(23,17).save(path));
    openshot::Tracker effect;TrackerTestBoxes(effect);effect.MaskReader(new openshot::QtImageReader(path.toStdString()));
    effect.trackedData->background_alpha.AddPoint(1,.1,openshot::LINEAR);effect.trackedData->background_alpha.AddPoint(9,.8,openshot::LINEAR);
    effect.trackedData->stroke_width.AddPoint(1,1,openshot::LINEAR);effect.trackedData->stroke_width.AddPoint(9,7,openshot::LINEAR);
    for(bool invert:{false,true}) {
        effect.mask_invert=invert;
        for(int n:{1,4,9}) CheckVulkanEffect(effect,pixels,source,n,3);
        const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
        for(int n:{2,5,8}) {
            auto frame=std::make_shared<openshot::Frame>(n,129,73,"transparent");frame->AddDeferredImage(source);
            { openshot::gpu_metrics::Scope scope(false);effect.ProcessFrame(frame,n); }REQUIRE(frame->GetDeferredImage());
        }
        const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);
    }
}
TEST_CASE("Vulkan tracker generated masks preserve rotation and multiple boxes", "[vulkan][hardware][effects][tracker-effects]") {
    if(!VulkanAvailable()) return;
    openshot::Tracker provider;provider.Id("tracker-provider");TrackerTestBoxes(provider);
    auto extra=std::make_shared<openshot::TrackedObjectBBox>();extra->SetBaseFPS(openshot::Fraction(24,1));
    extra->AddBox(1,.71f,.26f,.27f,.32f,-32);extra->AddBox(9,.39f,.41f,.3f,.4f,-21);provider.trackedObjects[1]=extra;
    openshot::Clip parent;parent.AddEffect(&provider);
    openshot::Brightness effect(openshot::Keyframe(.3),openshot::Keyframe(2));effect.MaskSourceId(provider.Id());parent.AddEffect(&effect);
    const auto pixels=PointEffectPixels(129,73);const auto source=openshot::UploadVulkanImage(pixels);
    for(double corner:{0.,5.,12.}) for(bool visible:{false,true}) for(bool invert:{false,true}) {
        provider.trackedData->background_corner=openshot::Keyframe(corner);extra->background_corner=openshot::Keyframe(corner);
        provider.trackedData->visible=openshot::Keyframe(visible);extra->visible=openshot::Keyframe(visible);effect.mask_invert=invert;
        for(int n:{1,4,9,19}) { INFO("corner="<<corner<<" visible="<<visible<<" invert="<<invert<<" frame="<<n);CheckVulkanEffect(effect,pixels,source,n,3,true); }
    }
}
TEST_CASE("Vulkan tracker preserves 4K output and its source", "[vulkan][hardware][effects][tracker-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(3840,2160);const auto source=openshot::UploadVulkanImage(pixels);
    openshot::Tracker effect;TrackerTestBoxes(effect);
    effect.trackedData->background_alpha=openshot::Keyframe(.37);effect.trackedData->stroke_width=openshot::Keyframe(6.7);
    effect.trackedData->background_corner=openshot::Keyframe(25);
    for(int n:{1,4,9}) CheckVulkanEffect(effect,pixels,source,n,3,true);
    CHECK(source->Materialize()==pixels);
}
#endif
#ifdef USE_OPENCV
TEST_CASE("Vulkan tracker preserves independent tiny fill and stroke coverage", "[vulkan][hardware][effects][tracker-effects]") {
    if(!VulkanAvailable()) return;
    for(const QSize size:{QSize(1,1),QSize(3,5),QSize(37,23)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());const auto source=openshot::UploadVulkanImage(pixels);
        openshot::Tracker effect;TrackerTestBoxes(effect);
        for(int layer:{0,1}) for(double width:{0.,7.2}) for(double corner:{0.,12.}) {
            effect.trackedData->stroke_alpha=openshot::Keyframe(layer==0?0:1);effect.trackedData->background_alpha=openshot::Keyframe(layer==0?1:0);
            effect.trackedData->stroke_width=openshot::Keyframe(width);effect.trackedData->background_corner=openshot::Keyframe(corner);
            for(int n:{1,4,9}) { INFO("size="<<size.width()<<" layer="<<layer<<" width="<<width<<" corner="<<corner<<" frame="<<n);CheckVulkanEffect(effect,pixels,source,n,3,true); }
        }
    }
}
#endif

#ifdef USE_OPENCV
TEST_CASE("Vulkan animated tracker and linked mask timeline remains copy free", "[vulkan][hardware][effects][tracker-effects]") {
    if(!VulkanAvailable()) return;
    const auto pixels=PointEffectPixels(128,96);const auto source=openshot::UploadVulkanImage(pixels);std::vector<QImage> expected;
    for(bool gpu:{false,true}) {
        struct Restore { std::string backend,device;~Restore(){openshot::ConfigureGpuEnvironment(backend,device);} }
        restore{std::getenv("OPENSHOT_RENDER_BACKEND")?std::getenv("OPENSHOT_RENDER_BACKEND"):"",std::getenv("OPENSHOT_VULKAN_DEVICE")?std::getenv("OPENSHOT_VULKAN_DEVICE"):""};
        openshot::ConfigureGpuEnvironment(gpu?"vulkan":"cpu",restore.device);
        PatternReader reader(source,gpu);openshot::Clip clip(&reader);
        openshot::Tracker tracker;TrackerTestBoxes(tracker);tracker.Id("chain-tracker");
        tracker.trackedData->ParentClip(&clip);
        tracker.trackedData->stroke_width=openshot::Keyframe(3.7);
        tracker.trackedData->background_alpha=openshot::Keyframe(.31);
        tracker.trackedData->background_corner=openshot::Keyframe(7);
        openshot::Brightness brightness(openshot::Keyframe(.12),openshot::Keyframe(2));brightness.MaskSourceId(tracker.Id());
        tracker.Order(10);brightness.Order(9);clip.AddEffect(&tracker);clip.AddEffect(&brightness);
        openshot::Timeline timeline(128,96,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);timeline.AddClip(&clip);timeline.Open();
        std::vector<std::shared_ptr<openshot::Frame>> frames{timeline.GetFrame(1)};openshot::ResetGpuMetrics();
        for(int n=2;n<=8;++n) { frames.push_back(timeline.GetFrame(n));if(gpu) REQUIRE(frames.back()->GetDeferredImage()); }
        if(gpu) { auto m=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(m["uploads"].asInt()==0);CHECK(m["readbacks"].asInt()==0); }
        for(size_t n=0;n<frames.size();++n) {
            auto actual=frames[n]->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);if(!gpu) { expected.push_back(actual);continue; }
            int error=0;double sum=0;for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x) { int d=std::abs(int(actual.constScanLine(y)[x])-int(expected[n].constScanLine(y)[x]));error=std::max(error,d);sum+=d; }
            INFO("frame="<<n+1<<" max="<<error<<" mean="<<sum/(actual.width()*actual.height()*4));CHECK(error<=3);CHECK(sum/(actual.width()*actual.height()*4)<=.1);
        }
        timeline.Close();
    }
    CHECK(source->Materialize()==pixels);
}
#endif

// The generators must work from the first frame: no uploaded seed or cached
// overlay is needed. Audio analysis remains shared CPU math, never video pixels.
#include "effects/AudioVisualization.h"
#include "effects/BeatSync.h"
#include "effects/Timer.h"
namespace {
void AddGeneratorAudio(std::shared_ptr<openshot::Frame> frame,int n) {
    const int samples=2000;frame->ResizeAudio(2,samples,48000,openshot::LAYOUT_STEREO);
    for(int channel=0;channel<2;++channel) {
        std::vector<float> audio(samples);
        for(int i=0;i<samples;++i) audio[i]=.6f*std::sin((i+n*31)*(channel?0.071:0.058))+.17f*std::cos(i*.23);
        frame->AddAudio(true,channel,0,audio.data(),samples,1);
    }
}
void CheckGenerator(openshot::EffectBase& cpu_effect,openshot::EffectBase& gpu_effect,const QImage& pixels,
    std::shared_ptr<const openshot::FrameImage> source,int n,int tolerance=3) {
    auto cpu=std::make_shared<openshot::Frame>(n,pixels.width(),pixels.height(),"transparent");
    auto gpu=std::make_shared<openshot::Frame>(n,pixels.width(),pixels.height(),"transparent");
    cpu->AddImage(std::make_shared<QImage>(pixels.copy()));gpu->AddDeferredImage(source);
    AddGeneratorAudio(cpu,n);AddGeneratorAudio(gpu,n);ProcessCpuEffect(cpu_effect,cpu,n);
    auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
    {openshot::gpu_metrics::Scope scope(false);gpu_effect.ProcessFrame(gpu,n);}
    auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
    CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);REQUIRE(gpu->GetDeferredImage());
    const auto expected=cpu->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied),actual=gpu->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    int error=0,ex=0,ey=0;double sum=0;
    for(int y=0;y<actual.height();++y) for(int x=0;x<actual.width()*4;++x) {
        int d=std::abs(int(expected.constScanLine(y)[x])-int(actual.constScanLine(y)[x]));sum+=d;if(d>error){error=d;ex=x/4;ey=y;}
    }
    INFO("frame="<<n<<" max="<<error<<" mean="<<sum/(actual.width()*actual.height()*4)<<" at="<<ex<<","<<ey);
    CHECK(error<=tolerance);
    CHECK(sum/(actual.width()*actual.height()*4)<=.25);
}
}
TEST_CASE("Vulkan BeatSync matches audio envelopes and seeks without transfers", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable()) return;
    auto pixels=PointEffectPixels(129,73);auto source=openshot::UploadVulkanImage(pixels);
    for(bool invert:{false,true}) {
        openshot::BeatSync cpu,gpu;cpu.invert=gpu.invert=invert;
        cpu.low_color=gpu.low_color=openshot::Color("#39278143");cpu.high_color=gpu.high_color=openshot::Color("#c3ef4587");
        for(int n:{1,2,3,19,20,2}) CheckGenerator(cpu,gpu,pixels,source,n,0);
    }
}
TEST_CASE("Vulkan Timer matches Qt outlines layouts and animated styles", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable()) return;
    EnsureVulkanTestApplication();
    for(const QSize size:{QSize(37,23),QSize(257,129),QSize(854,480)}) {
        auto pixels=PointEffectPixels(size.width(),size.height());auto source=openshot::UploadVulkanImage(pixels);
        openshot::Timer effect;effect.font_name="DejaVu Sans";effect.prefix="T ";effect.suffix=" !";
        effect.font_size=openshot::Keyframe(47);effect.show_background=true;effect.background_alpha=openshot::Keyframe(.37);
        for(int mode=0;mode<5;++mode) for(double stroke:{0.,.5,3.}) {
            effect.mode=mode;effect.format=mode;effect.stroke_width=openshot::Keyframe(stroke);
            INFO("width="<<size.width()<<" mode="<<mode<<" stroke="<<stroke);
            for(int n:{1,37,200}) CheckVulkanEffect(effect,pixels,source,n,3,true);
        }
    }
}
TEST_CASE("Vulkan audio visualization matches every mode style and background", "[vulkan][hardware][effects][remaining-effects][audio-vector]") {
    if(!VulkanAvailable()) return;
    auto pixels=PointEffectPixels(193,129);auto source=openshot::UploadVulkanImage(pixels);
    for(int mode=0;mode<=8;++mode) for(int style=0;style<4;++style) for(int color=0;color<2;++color) {
        openshot::AudioVisualization cpu,gpu;
        cpu.visualization_type=gpu.visualization_type=mode;cpu.style=gpu.style=style;
        cpu.color_mode=gpu.color_mode=color;cpu.background=gpu.background=(mode+style)%5;
        cpu.glow=gpu.glow=openshot::Keyframe(.37);cpu.detail=gpu.detail=openshot::Keyframe(.31);
        cpu.channel_layout=gpu.channel_layout=(mode+style)%4;
        INFO("mode="<<mode<<" style="<<style<<" color="<<color);
        CheckGenerator(cpu,gpu,pixels,source,3);
    }
}
#ifdef USE_OPENCV
TEST_CASE("Vulkan Mask handles linked generated sources and missing data", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable()) return;
    auto pixels=PointEffectPixels(129,73);auto source=openshot::UploadVulkanImage(pixels);
    openshot::Clip parent;openshot::Tracker provider;provider.Id("linked-tracker");TrackerTestBoxes(provider);parent.AddEffect(&provider);
    openshot::Mask effect;effect.MaskSourceId(provider.Id());parent.AddEffect(&effect);
    for(bool replace:{false,true}) for(bool invert:{false,true}) for(double contrast:{0.,7.,19.8}) {
        effect.replace_image=replace;effect.mask_invert=invert;effect.contrast=openshot::Keyframe(contrast);
        effect.brightness=openshot::Keyframe(-.2);
        for(int n:{1,4,9,19}) CheckVulkanEffect(effect,pixels,source,n,1,true);
    }
}
#endif
TEST_CASE("Vulkan generators create their first image without seed uploads", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable()) return;
    EnsureVulkanTestApplication();
    for(int kind=0;kind<3;++kind) for(bool audio:{false,true}) {
        openshot::BeatSync beat;openshot::AudioVisualization visual;openshot::Timer timer;
        auto* effect=kind==0?static_cast<openshot::EffectBase*>(&beat):kind==1?static_cast<openshot::EffectBase*>(&visual):static_cast<openshot::EffectBase*>(&timer);
        auto frame=std::make_shared<openshot::Frame>(1,37,23,"transparent");if(audio)AddGeneratorAudio(frame,1);
        const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
        {openshot::gpu_metrics::Scope scope(false);effect->ProcessFrame(frame,1);}
        const auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());
        REQUIRE(frame->GetDeferredImage());CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);
        CHECK(!frame->GetDeferredImage()->Materialize().isNull());
    }
}
TEST_CASE("Vulkan audio generators preserve tiny and 4K frames", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable()) return;
    for(QSize size:{QSize(1,1),QSize(3,5),QSize(3840,2160)}) {
        auto pixels=PointEffectPixels(size.width(),size.height());auto source=openshot::UploadVulkanImage(pixels);
        for(int mode:{1,2,3,4,5,6,7,8}) {
            openshot::AudioVisualization cpu,gpu;cpu.visualization_type=gpu.visualization_type=mode;
            cpu.detail=gpu.detail=openshot::Keyframe(.1);cpu.background=gpu.background=openshot::AUDIO_VISUALIZATION_BACKGROUND_SOURCE;
            INFO("size="<<size.width()<<"x"<<size.height()<<" mode="<<mode);
            CheckGenerator(cpu,gpu,pixels,source,2,3);
        }
        openshot::BeatSync cpu,gpu;CheckGenerator(cpu,gpu,pixels,source,1,0);
        CHECK(source->Materialize()==pixels);
    }
}
#ifdef USE_OPENCV
TEST_CASE("Vulkan Mask consumes detection segmentation and GPU reader masks", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable()) return;EnsureVulkanTestApplication();
    QTemporaryDir directory;auto detection=directory.filePath("detection.pb"),segmentation=directory.filePath("mask.pb");
    WriteDetectionData(detection);WriteObjectMaskData(segmentation,7,5,{{0,8,4,6,1,8,8},{35},{0,35}});
    const auto pixels=PointEffectPixels(129,73);auto source=openshot::UploadVulkanImage(pixels);
    openshot::Clip parent;openshot::ObjectDetection detector;detector.Id("detect");parent.AddEffect(&detector);REQUIRE(detector.LoadObjDetectdData(detection.toStdString()));
    openshot::ObjectMask object;object.Id("segment");parent.AddEffect(&object);REQUIRE(object.LoadObjMaskData(segmentation.toStdString()));
    openshot::Mask mask;parent.AddEffect(&mask);mask.brightness=openshot::Keyframe(-.25);mask.contrast=openshot::Keyframe(7);
    for(const char* id:{"detect","segment","missing"}) {
        mask.MaskSourceId(id);
        for(bool replace:{false,true})for(bool invert:{false,true}) {
            mask.replace_image=replace;mask.mask_invert=invert;
            for(int n:{1,2,3,19}) CheckVulkanEffect(mask,pixels,source,n,3,true);
        }
    }
    mask.MaskSourceId("");mask.MaskReader(new PatternReader(source,true));
    for(int n:{1,2,3}) CheckVulkanEffect(mask,pixels,source,n,1,true);
}
namespace {
class AudioPatternReader : public PatternReader {
public:
    using PatternReader::PatternReader;
    std::shared_ptr<openshot::Frame> GetFrame(int64_t n) override { auto frame=PatternReader::GetFrame(n);AddGeneratorAudio(frame,n);return frame; }
};
}
TEST_CASE("Vulkan remaining effects stay resident together in an animated timeline", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable())return;EnsureVulkanTestApplication();
    const auto pixels=PointEffectPixels(192,128);auto source=openshot::UploadVulkanImage(pixels);std::vector<QImage> expected;
    for(bool gpu:{false,true}) {
        struct Restore {std::string backend,device;~Restore(){openshot::ConfigureGpuEnvironment(backend,device);}}
        restore{std::getenv("OPENSHOT_RENDER_BACKEND")?std::getenv("OPENSHOT_RENDER_BACKEND"):"",std::getenv("OPENSHOT_VULKAN_DEVICE")?std::getenv("OPENSHOT_VULKAN_DEVICE"):""};
        openshot::ConfigureGpuEnvironment(gpu?"vulkan":"cpu",restore.device);
        AudioPatternReader reader(source,gpu);openshot::Clip clip(&reader);
        openshot::Tracker tracker;TrackerTestBoxes(tracker);tracker.Id("generator-tracker");
        openshot::Mask mask;mask.MaskSourceId(tracker.Id());mask.brightness=openshot::Keyframe(-.2);
        openshot::BeatSync beat;
        openshot::AudioVisualization visual;visual.background=openshot::AUDIO_VISUALIZATION_BACKGROUND_SOURCE;
        visual.visualization_type=openshot::AUDIO_VISUALIZATION_RADIAL;visual.glow.AddPoint(1,.1,openshot::LINEAR);visual.glow.AddPoint(9,.7,openshot::LINEAR);
        openshot::Timer timer;timer.font_name="DejaVu Sans";timer.font_size=openshot::Keyframe(43);timer.stroke_width=openshot::Keyframe(1.5);
        int order=10;for(auto* effect:std::vector<openshot::EffectBase*>{&tracker,&mask,&beat,&visual,&timer}){effect->Order(order--);clip.AddEffect(effect);}
        openshot::Timeline timeline(192,128,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);timeline.AddClip(&clip);timeline.Open();
        std::vector<std::shared_ptr<openshot::Frame>> frames;openshot::ResetGpuMetrics();
        for(int n=1;n<=8;++n){frames.push_back(timeline.GetFrame(n));if(gpu)REQUIRE(frames.back()->GetDeferredImage());}
        if(gpu){auto m=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(m["uploads"].asInt()==0);CHECK(m["readbacks"].asInt()==0);}
        for(size_t n=0;n<frames.size();++n){auto actual=frames[n]->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            if(!gpu){expected.push_back(actual);continue;}int error=0;double sum=0;
            for(int y=0;y<actual.height();++y)for(int x=0;x<actual.width()*4;++x){int d=std::abs(int(actual.constScanLine(y)[x])-int(expected[n].constScanLine(y)[x]));error=std::max(error,d);sum+=d;}
            INFO("frame="<<n+1<<" max="<<error<<" mean="<<sum/(actual.width()*actual.height()*4));CHECK(error<=3);CHECK(sum/(actual.width()*actual.height()*4)<=.25);
        }
        timeline.Close();
    }
    CHECK(source->Materialize()==pixels);
}
#endif
#include "VulkanPainter.h"
TEST_CASE("Vulkan cosmetic polylines preserve Qt reversal dropout pixels", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable())return;
    QPolygonF points;points<<QPointF(74.8679,56.8642)<<QPointF(76.0755,35.5976)<<QPointF(77.283,33.6169)<<QPointF(78.4906,35.3853);
    QImage pixels(97,73,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
    const auto source=openshot::UploadVulkanImage(pixels);
    for(bool aa:{false,true})for(double width:{.45,1.}) {
        QPen pen(QColor(60,140,230,122),width,Qt::SolidLine,Qt::FlatCap);
        auto expected=pixels.copy();QPainter cpu(&expected);cpu.setRenderHint(QPainter::Antialiasing,aa);cpu.setPen(pen);cpu.drawPolyline(points);cpu.end();
        openshot::VulkanPaintDevice device(97,73);QPainter gpu(&device);gpu.setRenderHint(QPainter::Antialiasing,aa);gpu.setPen(pen);gpu.drawPolyline(points);gpu.end();
        auto actual=device.Render(source)->Materialize();int error=0;
        for(int y=0;y<73;++y)for(int x=0;x<97*4;++x)error=std::max(error,std::abs(int(actual.constScanLine(y)[x])-int(expected.constScanLine(y)[x])));
        INFO("antialias="<<aa<<" width="<<width);CHECK(error<=1);
    }
}
namespace {
class ForeignFrameImage : public openshot::FrameImage {
    QImage pixels;
public:
    explicit ForeignFrameImage(QImage pixels):pixels(std::move(pixels)) {}
    int GetWidth() const override{return pixels.width();}
    int GetHeight() const override{return pixels.height();}
    int64_t GetBytes() const override{return pixels.sizeInBytes();}
    QImage Materialize() const override{return pixels.copy();}
};
}
TEST_CASE("Vulkan generators preserve CPU fallback for foreign surfaces", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable())return;EnsureVulkanTestApplication();
    auto pixels=PointEffectPixels(193,129);
    for(int kind:{0,1}){
        openshot::Timer timer;openshot::AudioVisualization visual;visual.background=openshot::AUDIO_VISUALIZATION_BACKGROUND_SOURCE;
        auto* effect=kind==0?static_cast<openshot::EffectBase*>(&timer):static_cast<openshot::EffectBase*>(&visual);
        auto cpu=std::make_shared<openshot::Frame>(3,193,129,"transparent"),fallback=std::make_shared<openshot::Frame>(3,193,129,"transparent");
        cpu->AddImage(std::make_shared<QImage>(pixels.copy()));fallback->AddDeferredImage(std::make_shared<ForeignFrameImage>(pixels));
        AddGeneratorAudio(cpu,3);AddGeneratorAudio(fallback,3);ProcessCpuEffect(*effect,cpu,3);effect->ProcessFrame(fallback,3);
        CHECK_FALSE(fallback->GetDeferredImage());CHECK(*fallback->GetImage()==*cpu->GetImage());
    }
}
TEST_CASE("Vulkan Mask keeps standalone CPU inputs functional", "[vulkan][hardware][effects][remaining-effects]") {
    if(!VulkanAvailable())return;
    auto pixels=PointEffectPixels(37,23),mask_pixels=PointEffectPixels(11,7);auto map=openshot::UploadVulkanImage(mask_pixels);
    openshot::Mask mask;mask.MaskReader(new PatternReader(map,false));mask.brightness=openshot::Keyframe(-.3);
    auto cpu=std::make_shared<openshot::Frame>(3,37,23,"transparent"),fallback=std::make_shared<openshot::Frame>(3,37,23,"transparent");
    cpu->AddImage(std::make_shared<QImage>(pixels.copy()));fallback->AddImage(std::make_shared<QImage>(pixels.copy()));
    ProcessCpuEffect(mask,cpu,3);mask.ProcessFrame(fallback,3);CHECK(*fallback->GetImage()==*cpu->GetImage());CHECK(*fallback->GetImage()!=pixels);
}
#include "FrameMapper.h"
TEST_CASE("Vulkan clip composition matches every Qt blend mode", "[vulkan][hardware][clip-mapper]") {
    if(!VulkanAvailable())return;
    for(QSize size:{QSize(1,1),QSize(3,5),QSize(129,73)}) {
        const auto pixels=PointEffectPixels(size.width(),size.height());auto background=pixels.mirrored(true,true);
        auto source=openshot::UploadVulkanImage(pixels),back=openshot::UploadVulkanImage(background);
        for(int mode=0;mode<=openshot::COMPOSITE_LAST;++mode) {
            QImage expected=background.copy();QPainter p(&expected);p.setCompositionMode(static_cast<QPainter::CompositionMode>(mode));p.drawImage(0,0,pixels);p.end();
            auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            std::shared_ptr<const openshot::FrameImage> result;
            {openshot::gpu_metrics::Scope scope(false);result=openshot::CompositeVulkanImage(source,back,mode);}
            auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);
            auto actual=result->Materialize();int error=0;
            for(int y=0;y<size.height();++y)for(int x=0;x<size.width()*4;++x)error=std::max(error,std::abs(int(actual.constScanLine(y)[x])-int(expected.constScanLine(y)[x])));
            INFO("size="<<size.width()<<"x"<<size.height()<<" mode="<<mode<<" error="<<error);CHECK(error<=1);
        }
    }
}

namespace {
class MappingTestReader : public openshot::DummyReader {
    std::vector<QImage> pixels;
    std::vector<std::shared_ptr<const openshot::FrameImage>> surfaces;
public:
    MappingTestReader(const std::vector<QImage>& images,openshot::Fraction fps,bool gpu)
        :DummyReader(fps,images.front().width(),images.front().height(),48000,2,5),pixels(images) {
        if(gpu)for(const auto& image:images)surfaces.push_back(openshot::UploadVulkanImage(image));
    }
    std::shared_ptr<openshot::Frame> GetFrame(int64_t n) override {
        auto frame=std::make_shared<openshot::Frame>(n,info.width,info.height,"transparent");
        const auto i=size_t(std::max<int64_t>(1,n)-1)%pixels.size();
        if(surfaces.empty())frame->AddImage(std::make_shared<QImage>(pixels[i]));else frame->AddDeferredImage(surfaces[i]);
        AddGeneratorAudio(frame,n);frame->ResizeAudio(2,openshot::Frame::GetSamplesPerFrame(n,info.fps,48000,2),48000,openshot::LAYOUT_STEREO);return frame;
    }
};
struct MappingBackend {
    std::string backend=std::getenv("OPENSHOT_RENDER_BACKEND")?std::getenv("OPENSHOT_RENDER_BACKEND"):"";
    std::string device=std::getenv("OPENSHOT_VULKAN_DEVICE")?std::getenv("OPENSHOT_VULKAN_DEVICE"):"";
    explicit MappingBackend(bool gpu){openshot::ConfigureGpuEnvironment(gpu?"vulkan":"cpu",device);}
    ~MappingBackend(){openshot::ConfigureGpuEnvironment(backend,device);}
};
void CheckClipPixels(const QImage& expected,const QImage& actual,int tolerance=3) {
    REQUIRE(actual.size()==expected.size());int error=0,ex=0,ey=0;double sum=0;
    for(int y=0;y<actual.height();++y)for(int x=0;x<actual.width()*4;++x){int d=std::abs(int(actual.constScanLine(y)[x])-int(expected.constScanLine(y)[x]));if(d>error){error=d;ex=x/4;ey=y;}sum+=d;}
    INFO("worst="<<ex<<","<<ey<<" CPU="<<expected.pixelColor(ex,ey).name(QColor::HexArgb).toStdString()<<" GPU="<<actual.pixelColor(ex,ey).name(QColor::HexArgb).toStdString());
    INFO("max="<<error<<" mean="<<sum/(actual.width()*actual.height()*4));CHECK(error<=tolerance);CHECK(sum/(actual.width()*actual.height()*4)<=.25);
}
}
TEST_CASE("Vulkan FrameMapper preserves field weaving rate changes and audio", "[vulkan][hardware][clip-mapper]") {
    if(!VulkanAvailable())return;
    const auto base=PointEffectPixels(97,73);std::vector<QImage> images{base,base.mirrored(true,false),base.mirrored(false,true)};
    for(auto rates:std::vector<std::pair<int,int>>{{24,30},{30,24},{25,30},{30,25},{24,24}})
    for(auto mode:{openshot::PULLDOWN_NONE,openshot::PULLDOWN_CLASSIC,openshot::PULLDOWN_ADVANCED}) {
        std::vector<QImage> expected;std::vector<std::vector<float>> audio;
        for(bool gpu:{false,true}) {
            MappingBackend backend(gpu);MappingTestReader reader(images,openshot::Fraction(rates.first,1),gpu);
            openshot::FrameMapper mapper(&reader,openshot::Fraction(rates.second,1),mode,44100,1,openshot::LAYOUT_MONO);mapper.Open();
            std::vector<std::shared_ptr<openshot::Frame>> frames;const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            for(int n:{1,2,3,4,5,9,8,7,1,19}){openshot::gpu_metrics::Scope scope(false);frames.push_back(mapper.GetFrame(n));if(gpu)REQUIRE(frames.back()->GetDeferredImage());}
            if(gpu){auto m=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(m["uploads"]==before["uploads"]);CHECK(m["readbacks"]==before["readbacks"]);}
            for(size_t i=0;i<frames.size();++i){auto frame=frames[i];auto pixels=frame->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
                if(!gpu){expected.push_back(pixels);audio.emplace_back(frame->GetAudioSamples(0),frame->GetAudioSamples(0)+frame->GetAudioSamplesCount());continue;}
                INFO("rate="<<rates.first<<"->"<<rates.second<<" mode="<<mode<<" index="<<i);CheckClipPixels(expected[i],pixels,0);
                REQUIRE(frame->GetAudioSamplesCount()==int(audio[i].size()));double error=0;for(int j=0;j<frame->GetAudioSamplesCount();++j)error=std::max(error,double(std::abs(frame->GetAudioSamples(0)[j]-audio[i][j])));CHECK(error<1e-6);
            }
            mapper.Close();
        }
    }
}
TEST_CASE("Vulkan clip modes preserve transformed CPU composition", "[vulkan][hardware][clip-mapper]") {
    if(!VulkanAvailable())return;
    auto pixels=PointEffectPixels(97,73);auto source=openshot::UploadVulkanImage(pixels);
    for(int mode=0;mode<=openshot::COMPOSITE_LAST;++mode)for(bool transform:{false,true}) {
        QImage expected;
        for(bool gpu:{false,true}) {
            MappingBackend backend(gpu);PatternReader reader(source,gpu);openshot::Clip clip(&reader);
            clip.composite=static_cast<openshot::CompositeType>(mode);clip.alpha=openshot::Keyframe(.63);
            if(transform){clip.rotation=openshot::Keyframe(13.2);clip.shear_x=openshot::Keyframe(.12);clip.scale_x=openshot::Keyframe(.81);clip.location_y=openshot::Keyframe(.07);}
            auto background=std::make_shared<openshot::Frame>(4,97,73,"transparent");auto bg=pixels.mirrored(true,true);
            if(gpu)background->AddDeferredImage(openshot::UploadVulkanImage(bg));else background->AddImage(std::make_shared<QImage>(bg));
            clip.Open();auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());std::shared_ptr<openshot::Frame> result;
            {openshot::gpu_metrics::Scope scope(false);result=clip.GetFrame(background,4);}
            if(gpu){REQUIRE(result->GetDeferredImage());auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);}
            auto actual=result->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            if(!gpu)expected=actual;else{INFO("mode="<<mode<<" transform="<<transform);CheckClipPixels(expected,actual);}
            clip.Close();
        }
    }
}
TEST_CASE("Vulkan clip waveform shortcut and disabled video preserve CPU pixels", "[vulkan][hardware][clip-mapper]") {
    if(!VulkanAvailable())return;
    auto pixels=PointEffectPixels(193,129);auto source=openshot::UploadVulkanImage(pixels);
    for(int mode=-2;mode<=8;++mode) {
        QImage expected;
        for(bool gpu:{false,true}) {
            MappingBackend backend(gpu);MappingTestReader reader({pixels},openshot::Fraction(24,1),gpu);openshot::Clip clip(&reader);
            openshot::Brightness brightness(openshot::Keyframe(.2),openshot::Keyframe(2));
            if(mode>=0){clip.Waveform(true);clip.WaveformMode(mode);clip.wave_color=openshot::Color("#c345a1d2");}
            else {clip.has_video=openshot::Keyframe(0);if(mode==-1)clip.AddEffect(&brightness);}
            clip.Open();auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());std::shared_ptr<openshot::Frame> frame;
            {openshot::gpu_metrics::Scope scope(false);frame=clip.GetFrame(4);}
            if(gpu){REQUIRE(frame->GetDeferredImage());auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);}
            auto actual=frame->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            if(!gpu)expected=actual;else{INFO("mode="<<mode);CheckClipPixels(expected,actual);}
            clip.Close();
        }
    }
}

TEST_CASE("Vulkan frame-number overlays preserve Qt text and stay resident", "[vulkan][hardware][clip-mapper][clip-text]") {
    if(!VulkanAvailable())return;EnsureVulkanTestApplication();
    auto pixels=PointEffectPixels(193,129);auto source=openshot::UploadVulkanImage(pixels);
    for(int display=1;display<=3;++display)for(int style=0;style<4;++style) {
        std::vector<QImage> expected;
        for(bool gpu:{false,true}) {
            MappingBackend backend(gpu);PatternReader reader(source,gpu);openshot::Clip clip(&reader);
            clip.display=static_cast<openshot::FrameDisplayType>(display);clip.alpha=openshot::Keyframe(.63);clip.Position(.75);clip.Start(.25);clip.End(4);
            if(style==1){clip.location_x=openshot::Keyframe(.071);clip.location_y=openshot::Keyframe(.043);}
            if(style==2){clip.rotation=openshot::Keyframe(13.2);clip.scale_x=openshot::Keyframe(.81);clip.location_y=openshot::Keyframe(.07);}
            if(style==3){clip.corner_radius=openshot::Keyframe(.2);clip.composite=openshot::COMPOSITE_MULTIPLY;}
            openshot::Timeline timeline(193,129,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);timeline.AddClip(&clip);clip.Open();
            const auto cold=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            {openshot::gpu_metrics::Scope scope(false);clip.GetFrame(1);}
            const auto before=openshot::stringToJson(openshot::GpuMetricsSnapshot());
            if(gpu){CHECK(before["readbacks"]==cold["readbacks"]);CHECK(before["uploads"].asInt64()-cold["uploads"].asInt64()<=1);}
            std::vector<std::shared_ptr<openshot::Frame>> frames;
            for(int n:{2,9,11,19,1000007}){openshot::gpu_metrics::Scope scope(false);frames.push_back(clip.GetFrame(n));if(gpu)REQUIRE(frames.back()->GetDeferredImage());}
            if(gpu){auto after=openshot::stringToJson(openshot::GpuMetricsSnapshot());CHECK(after["uploads"]==before["uploads"]);CHECK(after["readbacks"]==before["readbacks"]);}
            for(size_t i=0;i<frames.size();++i){auto actual=frames[i]->GetImage()->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
                if(!gpu)expected.push_back(actual);else{INFO("display="<<display<<" style="<<style<<" index="<<i);
                // Transformed unhinted glyph outlines differ slightly from Qt/FreeType's
                // rasterizer. Keep the tight image tolerance for all other cases and
                // retain the same mean-error bound for transformed text.
                CheckClipPixels(expected[i],actual,style==2?20:3);}}
            clip.Close();
        }
    }
}

TEST_CASE("Vulkan distort matches CPU presets animated points and folds", "[vulkan][hardware][effects][distort]") {
    if (!VulkanAvailable()) return;
    QImage pixels(96,64,QImage::Format_RGBA8888_Premultiplied);
    for (int y=0;y<pixels.height();++y) for (int x=0;x<pixels.width();++x)
        pixels.setPixelColor(x,y,QColor((x*7)%256,(y*11)%256,(x*3+y*5)%256,(x%4)*85));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::Distort effect;
    effect.strength.AddPoint(1,-0.5,openshot::LINEAR);
    effect.strength.AddPoint(9,1.0,openshot::LINEAR);
    for (int preset=0;preset<=6;++preset) {
        effect.preset=preset;
        INFO("preset=" << preset);
        for (int number : {1,5,9,3}) CheckVulkanEffect(effect,pixels,source,number,2);
    }
    effect.points_x[5].AddPoint(1,0.0,openshot::LINEAR);
    effect.points_x[5].AddPoint(9,0.8,openshot::LINEAR);
    effect.points_y[6]=openshot::Keyframe(-0.8);
    for (int number : {1,5,9,3}) CheckVulkanEffect(effect,pixels,source,number,2);
}
TEST_CASE("Vulkan distort retains queued meshes and handles small frames", "[vulkan][hardware][effects][distort]") {
    if (!VulkanAvailable()) return;
    for (int size : {1,2,17}) {
        QImage pixels(size,size,QImage::Format_RGBA8888_Premultiplied);
        pixels.fill(QColor(240,80,20,128));
        auto source=openshot::UploadVulkanImage(pixels);
        openshot::Distort effect;
        effect.preset=3;
        CheckVulkanEffect(effect,pixels,source,1,2);
    }
    QImage pixels(96,64,QImage::Format_RGBA8888_Premultiplied);
    pixels.fill(QColor(220,90,30,170));
    auto source=openshot::UploadVulkanImage(pixels);
    openshot::Distort effect;
    std::vector<std::shared_ptr<openshot::Frame>> queued;
    std::vector<QImage> expected;
    auto readbacks=openshot::VulkanFrameReadbackCount();
    for (int i=0;i<12;++i) {
        std::fill(effect.points_x.begin(), effect.points_x.end(), openshot::Keyframe(i/48.0));
        auto cpu=std::make_shared<openshot::Frame>(1,96,64,"transparent");
        cpu->AddImage(std::make_shared<QImage>(pixels));
        ProcessCpuEffect(effect,cpu,1); expected.push_back(cpu->GetImage()->copy());
        auto gpu=std::make_shared<openshot::Frame>(1,96,64,"transparent");
        gpu->AddDeferredImage(source); effect.ProcessFrame(gpu,1);
        REQUIRE(gpu->GetDeferredImage()); queued.push_back(gpu);
    }
    CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
    for (size_t i=0;i<queued.size();++i) CHECK(*queued[i]->GetImage()==expected[i]);
}

TEST_CASE("Vulkan distort uploads CPU clip frames and preserves identity surfaces", "[vulkan][hardware][effects][distort]") {
    if (!VulkanAvailable()) return;
    QImage pixels(48,32,QImage::Format_RGBA8888_Premultiplied);
    pixels.fill(QColor(240,80,20,128));
    openshot::Clip clip;
    openshot::Distort effect;
    clip.AddEffect(&effect);
    auto frame=std::make_shared<openshot::Frame>(1,48,32,"transparent");
    frame->AddImage(std::make_shared<QImage>(pixels));
    effect.preset=1;
    auto readbacks=openshot::VulkanFrameReadbackCount();
    effect.ProcessFrame(frame,1);
    REQUIRE(frame->GetDeferredImage());
    CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
    auto surface=frame->GetDeferredImage();
    effect.strength=openshot::Keyframe(0.0);
    effect.ProcessFrame(frame,2);
    CHECK(frame->GetDeferredImage()==surface);
}

TEST_CASE("Vulkan distort keeps triangle edges covered at HD and 4K", "[vulkan][hardware][effects][distort]") {
    if (!VulkanAvailable()) return;
    for (int width : {1920,3840}) {
        QImage pixels(width,width*9/16,QImage::Format_RGBA8888_Premultiplied);
        for (int y=0;y<pixels.height();++y) for (int x=0;x<width;++x)
            pixels.setPixelColor(x,y,QColor(x*255/width,y*255/pixels.height(),180,170));
        auto source=openshot::UploadVulkanImage(pixels);
        openshot::Distort effect;
        for (int preset : {2,5}) {
            effect.preset=preset;
            INFO("width=" << width << " preset=" << preset);
            CheckVulkanEffect(effect,pixels,source,1,2);
        }
    }
}
