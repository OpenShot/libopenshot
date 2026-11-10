// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Clip.h"
#include "Timeline.h"
#include "FFmpegWriter.h"
#ifdef OPENSHOT_ENABLE_GPU_FRAME
#include "VulkanFrameImage.h"
#endif
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

// Identical full-resolution rendering + NVENC workload for CPU/GPU builds.
// Optional audio input exercises timeline mixing and muxing. Output is replaced.
int main(int argc,char** argv) {
    if(argc<3 || argc>7){
        std::cerr<<"Usage: openshot-export-benchmark VIDEO OUTPUT.mp4 [FRAMES=120] [LAYERS=2] [AUDIO|-] [CODEC=h264_nvenc]\n";
        return 2;
    }
    try {
        const int count=argc>3?std::stoi(argv[3]):120;
        const int layers=argc>4?std::stoi(argv[4]):2;
        if(count<1 || layers<1 || layers>16) throw std::invalid_argument("Invalid frame/layer count");
        std::vector<std::unique_ptr<openshot::Clip>> clips;
        openshot::Timeline timeline(3840,2160,openshot::Fraction(24,1),48000,2,openshot::LAYOUT_STEREO);
        timeline.SetMaxSize(3840,2160);
        for(int index=0;index<layers;++index){
            auto clip=std::make_unique<openshot::Clip>(argv[1]);
            clip->Layer(index);
            if(index){
                clip->scale_x=openshot::Keyframe(0.7);
                clip->scale_y=openshot::Keyframe(0.65);
                clip->shear_x=openshot::Keyframe(0.1);
                clip->rotation=openshot::Keyframe(12.0*index);
                clip->alpha=openshot::Keyframe(0.6);
            }
            timeline.AddClip(clip.get());clips.push_back(std::move(clip));
        }
        if(argc>5 && std::string(argv[5])!="-"){
            auto audio=std::make_unique<openshot::Clip>(argv[5]);audio->Layer(-1);
            // Do not render cover art embedded in an audio file.
            audio->has_video=openshot::Keyframe(0.0);
            timeline.AddClip(audio.get());clips.push_back(std::move(audio));
        }
        timeline.Open();
        openshot::FFmpegWriter writer(argv[2]);
        writer.SetVideoOptions(true,argc>6?argv[6]:"h264_nvenc",openshot::Fraction(24,1),3840,2160,openshot::Fraction(1,1),false,false,40000000);
        writer.SetAudioOptions(true,"aac",48000,2,openshot::LAYOUT_STEREO,192000);
        writer.Open();
        int deferred=0;
        std::shared_ptr<openshot::Frame> last;
#ifdef OPENSHOT_ENABLE_GPU_FRAME
        const auto readbacks=openshot::VulkanFrameReadbackCount();
#endif
        const auto start=std::chrono::steady_clock::now();
        for(int n=1;n<=count;++n){
            last=timeline.GetFrame(n);
#ifdef OPENSHOT_ENABLE_GPU_FRAME
            deferred+=bool(last->GetDeferredImage());
#endif
            writer.WriteFrame(last);
        }
        writer.Close();
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"frames="<<count<<" layers="<<layers<<" size=3840x2160 codec="<<writer.info.vcodec<<" deferred_frames="<<deferred
#ifdef OPENSHOT_ENABLE_GPU_FRAME
                 <<" gpu_readbacks="<<openshot::VulkanFrameReadbackCount()-readbacks
#endif
                 <<" seconds="<<seconds<<" fps="<<count/seconds<<std::endl;
        if(!last->GetImage()->save(QString::fromStdString(std::string(argv[2])+".last.png")))
            throw std::runtime_error("Failed to save reference image");
        timeline.Close();
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
