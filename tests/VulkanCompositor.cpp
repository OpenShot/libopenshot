#ifdef __APPLE__
#include "MacVulkanInterop.h"
#include <CoreVideo/CoreVideo.h>
#endif
// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
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
#include <algorithm>
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
        WARN("Skipping Vulkan test: OPENSHOT_RENDER_BACKEND=cpu");
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
        const auto expected = expected_frame->GetImage();
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
    CHECK(openshot::VulkanRenderingEnabled());
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
    std::shared_ptr<const openshot::FrameImage> source, int number, int tolerance) {
    auto cpu=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
    auto gpu=std::make_shared<openshot::Frame>(number,pixels.width(),pixels.height(),"transparent");
    cpu->AddImage(std::make_shared<QImage>(pixels.copy()));
    gpu->AddDeferredImage(source);
    ProcessCpuEffect(effect,cpu,number);
    const auto readbacks=openshot::VulkanFrameReadbackCount();
    effect.ProcessFrame(gpu,number);
    REQUIRE(gpu->GetDeferredImage());
    CHECK(openshot::VulkanFrameReadbackCount()==readbacks);
    auto expected=cpu->GetImage(),actual=gpu->GetImage();
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
        uint64_t counted=0;
        for (auto bin : scope.GetVideoHistogramLumaBins()) counted+=bin;
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
        const int x0=(index%4)*80+30,y0=(index/4)*48+18;
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
        // Hardware decoders may decline these small charts (VideoToolbox on an
        // Intel Mac rejects the 320x192 H.264 one) and FFmpeg then decodes in
        // software. Mac 10-bit surfaces are covered with synthetic
        // CVPixelBuffers below.
        if(!gpu->GetDeferredImage()) {
            WARN("Skipping Vulkan test: this device does not decode " << chart << " on the GPU");
            continue;
        }
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
