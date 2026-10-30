#ifdef __APPLE__
#include "MacVulkanInterop.h"
#endif
// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "VulkanCompositor.h"
#include "VulkanFrameImage.h"
#include "VulkanEncodeConverter.h"
#include "GpuDevices.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include <QTemporaryDir>
#include <algorithm>
#include "Clip.h"
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
    gpu.reset();cpu.reset();pool.reset();device.reset();source.reset();
    CHECK(first->GetBytes() >= 32 * 32 * 4);
    QImage image=first->Materialize();
    const auto blended=image.pixelColor(12,12);
    CHECK(blended.red()>=126);CHECK(blended.red()<=129);
    CHECK(blended.green()==0);
    CHECK(blended.blue()>=126);CHECK(blended.blue()<=129);
    CHECK(blended.alpha()==255);
    CHECK(image.pixelColor(0,0)==QColor(Qt::blue));
    CHECK(second->Materialize().pixelColor(0,0)==QColor(Qt::red));
    // Export conversion remains on the GPU and produces limited-range BT.601 NV12.
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
        auto high=processor.Process(gpu.get(),size.width(),size.height(),true);
        REQUIRE(std::dynamic_pointer_cast<const openshot::VulkanFrameImage>(high));
        auto high_image=high->Materialize();
        auto low_image=processor.Process(gpu.get(),size.width(),size.height(),false)->Materialize();
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
