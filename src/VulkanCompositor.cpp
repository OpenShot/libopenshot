// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VulkanCompositor.h"
#include "VulkanEncodeConverter.h"
#include "GpuDevices.h"
#include "VulkanFrameImage.h"
#ifdef __APPLE__
#include "MacVulkanInterop.h"
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <map>
#include <mutex>
#include <stdexcept>
#include <sstream>
#include <vector>
extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/error.h>
}
// SPIR-V generated from src/vulkan/*.comp at build time.
#include "affine.comp.h"
#include "encode_nv12.comp.h"
namespace openshot {
namespace {
std::atomic<uint64_t> pipeline_creations{0};
void VkCheck(VkResult r, const char* operation) {
    if(r == VK_SUCCESS) return;
    ReportVulkanResult(r);
    throw std::runtime_error(std::string(operation)+": "+VulkanResultName(r));
}
void AvCheck(int r, const char* operation) {
    if(r < 0) {
        char error[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(r, error, sizeof(error));
        throw std::runtime_error(std::string(operation)+": FFmpeg error "+std::to_string(r)+" ("+error+")");
    }
}
struct FrameDelete { void operator()(AVFrame* f) const { av_frame_free(&f); } };
using FramePtr=std::unique_ptr<AVFrame,FrameDelete>;
// Submissions in flight per pipeline before Render waits for the oldest one.
constexpr int kSlots=4;
// Output pools are shared by every thread so ReleaseVulkanMemory() can drop them.
// A pool keeps freed frames for reuse; FFmpeg frees a dropped pool's memory once
// its last frame is released.
struct OutputPools {
    std::mutex mutex;
    struct Entry { AVBufferRef* frames=nullptr; uint64_t used=0; };
    std::map<std::tuple<AVHWDeviceContext*,int,int,bool>,Entry> pools;
    uint64_t clock=0;
};
// Never destroyed: at process exit the Windows NVIDIA driver may already be
// unloading, and freeing Vulkan frames from a static destructor crashes it.
OutputPools& SharedPools(){ static auto* pools=new OutputPools; return *pools; }
constexpr VkImageUsageFlags kOutputUsage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
// Returns a new reference the caller must release.
AVBufferRef* OutputPool(AVBufferRef* device_ref,int w,int h,bool encode_nv12){
    auto& shared=SharedPools();
    const std::lock_guard<std::mutex> lock(shared.mutex);
    auto& entry=shared.pools[{reinterpret_cast<AVHWDeviceContext*>(device_ref->data),w,h,encode_nv12}];
    entry.used=++shared.clock;
    auto& pool=entry.frames;
    if(!pool){
        AVBufferRef* frames=av_hwframe_ctx_alloc(device_ref);
        if(!frames) throw std::bad_alloc();
        auto* fc=reinterpret_cast<AVHWFramesContext*>(frames->data);
        fc->format=AV_PIX_FMT_VULKAN; fc->sw_format=encode_nv12?AV_PIX_FMT_NV12:AV_PIX_FMT_RGBA;
        fc->width=w; fc->height=h;
        auto* vc=reinterpret_cast<AVVulkanFramesContext*>(fc->hwctx);
        if(encode_nv12) vc->flags=AV_VK_FRAME_FLAG_DISABLE_MULTIPLANE;
        vc->usage=static_cast<VkImageUsageFlagBits>(kOutputUsage);
        const int status=av_hwframe_ctx_init(frames);
        if(status<0){
            av_buffer_unref(&frames); shared.pools.erase({reinterpret_cast<AVHWDeviceContext*>(device_ref->data),w,h,encode_nv12});
            AvCheck(status,"Initialize Vulkan compositor output");
        }
        pool=frames;
    }
    // Resizing effects can produce a new size every frame. Keep retained pools
    // bounded; frames still in use retain their own references to evicted pools.
    while(shared.pools.size()>16) {
        auto oldest=std::min_element(shared.pools.begin(),shared.pools.end(),
            [](const auto& a,const auto& b){return a.second.used<b.second.used;});
        av_buffer_unref(&oldest->second.frames);
        shared.pools.erase(oldest);
    }
    AVBufferRef* ref=av_buffer_ref(pool);
    if(!ref) throw std::bad_alloc();
    return ref;
}
struct EffectParameters {
    int mode; // 1 saturation, 2 crop, 3 grading, 4 mask
    std::array<float,9> values{};
    const VulkanGradeParameters* grade=nullptr;
};
struct Parameters {
    float matrix_x[4], matrix_y[4], coeffs[4], extra[4], clear[4];
    int sizes[4], modes[4];
    float effect[4];
};
static_assert(sizeof(VulkanGradeParameters)==1045*sizeof(float), "Grading buffer must match shader scalar layout");
static_assert(sizeof(Parameters)==128, "Vulkan guarantees 128 bytes of push constants");
struct LockedFrame {
    AVHWFramesContext* context;
    AVVulkanFramesContext* vkcontext;
    AVVkFrame* frame;
    explicit LockedFrame(const AVFrame* f) {
        context=reinterpret_cast<AVHWFramesContext*>(f->hw_frames_ctx->data);
        vkcontext=reinterpret_cast<AVVulkanFramesContext*>(context->hwctx);
        frame=reinterpret_cast<AVVkFrame*>(f->data[0]);
        vkcontext->lock_frame(context,frame);
    }
    ~LockedFrame(){vkcontext->unlock_frame(context,frame);}
};
class Pipeline {
    AVBufferRef* device_ref=nullptr;
    AVHWDeviceContext* device_context=nullptr;
    AVVulkanDeviceContext* vkcontext=nullptr;
    VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;
    int queue_family=-1;
    VkCommandPool commands=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptors=VK_NULL_HANDLE;
    VkDescriptorPool pool=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    // Consumers order submissions through the frames' timeline semaphores.
    // Keep encoder-owned targets alive until their views and work are retired.
    struct Slot {
        VkDescriptorSet set=VK_NULL_HANDLE;
        VkCommandBuffer cmd=VK_NULL_HANDLE;
        VkFence fence=VK_NULL_HANDLE;
        bool busy=false;
        VkBuffer grade_buffer=VK_NULL_HANDLE;
        VkDeviceMemory grade_memory=VK_NULL_HANDLE;
        void* grade_data=nullptr;
        bool grade_coherent=false;
        std::vector<VkImageView> views;
        FramePtr target;
        std::vector<FramePtr> inputs;
    };
    std::array<Slot,kSlots> ring;
    int next_slot=0;
    bool encode_nv12;
    void CreateGradeBuffer(Slot& slot) {
        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer.size=sizeof(VulkanGradeParameters);
        buffer.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        VkCheck(vkCreateBuffer(device,&buffer,nullptr,&slot.grade_buffer),"Create grading parameter buffer");
        VkMemoryRequirements requirements;
        vkGetBufferMemoryRequirements(device,slot.grade_buffer,&requirements);
        VkPhysicalDeviceMemoryProperties memory;
        vkGetPhysicalDeviceMemoryProperties(vkcontext->phys_dev,&memory);
        uint32_t type=UINT32_MAX;
        for(uint32_t i=0;i<memory.memoryTypeCount;++i) {
            const auto flags=memory.memoryTypes[i].propertyFlags;
            if((requirements.memoryTypeBits&(1u<<i)) && (flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                type=i;
                if(flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) break;
            }
        }
        if(type==UINT32_MAX) throw std::runtime_error("No host-visible grading parameter memory");
        slot.grade_coherent=memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;
        VkCheck(vkAllocateMemory(device,&allocation,nullptr,&slot.grade_memory),"Allocate grading parameter memory");
        VkCheck(vkBindBufferMemory(device,slot.grade_buffer,slot.grade_memory,0),"Bind grading parameter memory");
        VkCheck(vkMapMemory(device,slot.grade_memory,0,VK_WHOLE_SIZE,0,&slot.grade_data),"Map grading parameters");
    }
public:
    Pipeline(AVBufferRef* device_owner,bool encode=false):encode_nv12(encode) {
        device_ref=av_buffer_ref(device_owner);
        if(!device_ref) throw std::bad_alloc();
        device_context=reinterpret_cast<AVHWDeviceContext*>(device_ref->data);
        vkcontext=reinterpret_cast<AVVulkanDeviceContext*>(device_context->hwctx);
        device=vkcontext->act_dev;
        queue_family=vkcontext->queue_family_comp_index;
        if(queue_family<0) throw std::runtime_error("No Vulkan compute queue");
        try {
            vkGetDeviceQueue(device,queue_family,0,&queue);
            VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            ci.queueFamilyIndex=queue_family; ci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            VkCheck(vkCreateCommandPool(device,&ci,nullptr,&commands),"Create command pool");
            std::array<VkDescriptorSetLayoutBinding,6> bindings{};
            for(int i=0;i<6;++i) {
                bindings[i].binding=i; bindings[i].descriptorCount=1;
                bindings[i].descriptorType=i==5?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:(i==4 || (encode_nv12 && i==3))?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
            }
            VkDescriptorSetLayoutCreateInfo di{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            di.bindingCount=6; di.pBindings=bindings.data();
            VkCheck(vkCreateDescriptorSetLayout(device,&di,nullptr,&descriptors),"Create descriptors");
            VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Parameters)};
            VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            li.setLayoutCount=1; li.pSetLayouts=&descriptors; li.pushConstantRangeCount=1; li.pPushConstantRanges=&range;
            VkCheck(vkCreatePipelineLayout(device,&li,nullptr,&layout),"Create pipeline layout");
            std::array<VkDescriptorPoolSize,3> sizes{{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4*kSlots},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2*kSlots},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,kSlots}}};
            VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pi.maxSets=kSlots; pi.poolSizeCount=3; pi.pPoolSizes=sizes.data();
            VkCheck(vkCreateDescriptorPool(device,&pi,nullptr,&pool),"Create descriptor pool");
            for(auto& slot:ring){
                if(!encode_nv12) CreateGradeBuffer(slot);
                VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                ai.descriptorPool=pool; ai.descriptorSetCount=1; ai.pSetLayouts=&descriptors;
                VkCheck(vkAllocateDescriptorSets(device,&ai,&slot.set),"Allocate descriptors");
                VkCommandBufferAllocateInfo ci{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                ci.commandPool=commands; ci.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ci.commandBufferCount=1;
                VkCheck(vkAllocateCommandBuffers(device,&ci,&slot.cmd),"Allocate affine commands");
                VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                VkCheck(vkCreateFence(device,&fi,nullptr,&slot.fence),"Create affine fence");
            }
            VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            si.magFilter=si.minFilter=VK_FILTER_LINEAR;
            si.addressModeU=si.addressModeV=si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            VkCheck(vkCreateSampler(device,&si,nullptr,&sampler),"Create sampler");
            VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            mi.codeSize=encode_nv12?sizeof(openshot_encode_nv12_spv):sizeof(openshot_affine_spv);
            mi.pCode=encode_nv12?openshot_encode_nv12_spv:openshot_affine_spv;
            VkShaderModule module;
            VkCheck(vkCreateShaderModule(device,&mi,nullptr,&module),"Create shader module");
            VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pci.layout=layout; pci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; pci.stage.module=module; pci.stage.pName="main";
            const auto status=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&pci,nullptr,&pipeline);
            vkDestroyShaderModule(device,module,nullptr);
            VkCheck(status,"Create affine pipeline");
            pipeline_creations.fetch_add(1,std::memory_order_relaxed);
        }catch(...){Destroy();throw;}
    }
    ~Pipeline(){Destroy();}
    void Destroy(){
        for(auto& slot:ring){
            Retire(slot);
            if(slot.grade_data) vkUnmapMemory(device,slot.grade_memory);
            if(slot.grade_buffer) vkDestroyBuffer(device,slot.grade_buffer,nullptr);
            if(slot.grade_memory) vkFreeMemory(device,slot.grade_memory,nullptr);
            if(slot.fence) vkDestroyFence(device,slot.fence,nullptr);
        }
        if(pipeline) vkDestroyPipeline(device,pipeline,nullptr);
        if(sampler) vkDestroySampler(device,sampler,nullptr);
        if(pool) vkDestroyDescriptorPool(device,pool,nullptr);
        if(layout) vkDestroyPipelineLayout(device,layout,nullptr);
        if(descriptors) vkDestroyDescriptorSetLayout(device,descriptors,nullptr);
        if(commands) vkDestroyCommandPool(device,commands,nullptr);
        av_buffer_unref(&device_ref);
    }
    // Wait for a slot's previous submission, then release its views.
    void Retire(Slot& slot){
        if(slot.busy){
            vkWaitForFences(device,1,&slot.fence,VK_TRUE,UINT64_MAX);
            vkResetFences(device,1,&slot.fence);
            slot.busy=false;
        }
        for(auto view:slot.views) vkDestroyImageView(device,view,nullptr);
        slot.views.clear();
        slot.target.reset();
        slot.inputs.clear();
    }
    VkImageView View(VkImage image,VkFormat format,VkImageAspectFlags aspect){
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=image; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=format;
        vi.subresourceRange.aspectMask=aspect; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1;
        VkImageView view;
        VkCheck(vkCreateImageView(device,&vi,nullptr,&view),"Create affine image view");
        return view;
    }
    std::shared_ptr<const FrameImage> Render(const VulkanFrameImage& input,const VulkanFrameImage* background,
        int width,int height,const QTransform& matrix,double opacity,const QColor& clear,bool high_quality=false,const AVFrame* target=nullptr,
        const EffectParameters* effect=nullptr) {
        const auto* src=input.GetFrame();
        const auto* src_fc=reinterpret_cast<const AVHWFramesContext*>(src->hw_frames_ctx->data);
        if(src_fc->device_ctx!=device_context) throw std::runtime_error("Vulkan source device mismatch");
        const AVFrame* back=background?background->GetFrame():nullptr;
        if(back){
            const auto* bc=reinterpret_cast<const AVHWFramesContext*>(back->hw_frames_ctx->data);
            if(bc->device_ctx!=device_context || bc->sw_format!=AV_PIX_FMT_RGBA || back->width!=width || back->height!=height)
                throw std::runtime_error("Vulkan background must be matching RGBA surface");
        }
        FramePtr output(av_frame_alloc());
        if(!output) throw std::bad_alloc();
        if(target){
            auto* target_fc=target->hw_frames_ctx?reinterpret_cast<AVHWFramesContext*>(target->hw_frames_ctx->data):nullptr;
            if(!encode_nv12 || !target_fc || target->format!=AV_PIX_FMT_VULKAN ||
               target_fc->sw_format!=AV_PIX_FMT_NV12 || target_fc->device_ctx!=device_context ||
               target->width!=width || target->height!=height)
                throw std::invalid_argument("Encoder target must be matching NV12 on the rendering device");
            const auto* target_vk=static_cast<const AVVulkanFramesContext*>(target_fc->hwctx);
            if(!(target_vk->usage&VK_IMAGE_USAGE_STORAGE_BIT))
                throw std::runtime_error("Encoder target cannot be written by a Vulkan shader");
            AvCheck(av_frame_ref(output.get(),target),"Retain encoder target");
        }else{
#ifdef __APPLE__
            output.reset(AllocateMacVulkanFrame(device_ref,width,height,encode_nv12));
#else
            AVBufferRef* pool=OutputPool(device_ref,width,height,encode_nv12);
            const int allocated=av_hwframe_get_buffer(pool,output.get(),0);
            std::string operation="Allocate compositor output";
            if(allocated<0){
                const auto* fc=reinterpret_cast<const AVHWFramesContext*>(pool->data);
                const auto* vc=reinterpret_cast<const AVVulkanFramesContext*>(fc->hwctx);
                std::ostringstream detail;
                detail << operation << " [" << width << 'x' << height
                       << ", format=" << (encode_nv12?"NV12":"RGBA")
                       << ", usage=0x" << std::hex << vc->usage
                       << ", image_flags=0x" << vc->img_flags << std::dec
                       << ", tiling=" << vc->tiling << ']';
                operation=detail.str();
            }
            av_buffer_unref(&pool);
            AvCheck(allocated,operation.c_str());
#endif
        }
        AvCheck(av_frame_copy_props(output.get(),src),"Copy compositor metadata");
        output->colorspace=encode_nv12?AVCOL_SPC_SMPTE170M:AVCOL_SPC_RGB; output->color_range=encode_nv12?AVCOL_RANGE_MPEG:AVCOL_RANGE_JPEG;
        if(encode_nv12 && src_fc->sw_format!=AV_PIX_FMT_RGBA) throw std::runtime_error("Encoder conversion requires RGBA");
        Parameters p{};
        bool invertible=false;
        const auto inverse=matrix.inverted(&invertible);
        if(!invertible) opacity=0;
        p.matrix_x[0]=inverse.m11();p.matrix_x[1]=inverse.m21();p.matrix_x[2]=inverse.dx();
        p.matrix_y[0]=inverse.m12();p.matrix_y[1]=inverse.m22();p.matrix_y[2]=inverse.dy();
        p.sizes[0]=width;p.sizes[1]=height;p.sizes[2]=src->width;p.sizes[3]=src->height;
        p.extra[3]=std::clamp(opacity,0.0,1.0);
        p.clear[3]=clear.alphaF();p.clear[0]=clear.redF()*p.clear[3];p.clear[1]=clear.greenF()*p.clear[3];p.clear[2]=clear.blueF()*p.clear[3];
        p.modes[1]=back?1:0;
        p.modes[2]=high_quality?1:0;
        if(effect) {
            p.modes[3]=effect->mode;
            std::copy_n(effect->values.begin(),4,p.effect);
            if(effect->mode==3 || effect->mode==5) {
                // Grading uses no background. Reuse clear and the unused matrix
                // component to stay within Vulkan's guaranteed 128-byte limit.
                std::copy_n(effect->values.begin()+4,4,p.clear);
                p.matrix_x[3]=effect->values[8];
            }
        }
        int plane_count=0;
        if(src_fc->sw_format==AV_PIX_FMT_RGBA){p.modes[0]=input.IsPremultiplied()?2:3;plane_count=1;}
        else if(src_fc->sw_format==AV_PIX_FMT_NV12){p.modes[0]=1;plane_count=1;}
        else if(src_fc->sw_format==AV_PIX_FMT_YUV420P){p.modes[0]=0;plane_count=3;}
        else throw std::runtime_error("Vulkan affine renderer supports RGBA, NV12 and YUV420P");
        double kr=0.299,kb=0.114;
        if(src->colorspace==AVCOL_SPC_BT709){kr=0.2126;kb=0.0722;}
        else if(src->colorspace==AVCOL_SPC_BT2020_NCL){kr=0.2627;kb=0.0593;}
        else if(src->colorspace==AVCOL_SPC_SMPTE240M){kr=0.212;kb=0.087;}
        const bool full=src->color_range==AVCOL_RANGE_JPEG;
        const double cscale=full?1.0:255.0/224.0;
        p.coeffs[0]=full?1.0:255.0/219.0;p.extra[1]=full?0.0:16.0/255.0;p.extra[2]=128.0/255.0;
        p.coeffs[1]=2*(1-kr)*cscale;p.coeffs[2]=-2*kb*(1-kb)/(1-kr-kb)*cscale;
        p.coeffs[3]=-2*kr*(1-kr)/(1-kr-kb)*cscale;p.extra[0]=2*(1-kb)*cscale;
        // Retire before locking inputs: releasing the last imported surface can
        // wait on that frame's synchronization mutex.
        Slot& slot=ring[next_slot];
        next_slot=(next_slot+1)%kSlots;
        Retire(slot);
        if(effect && effect->grade) {
            // Each in-flight submission owns its parameters until its fence retires.
            std::memcpy(slot.grade_data,effect->grade,sizeof(VulkanGradeParameters));
            if(!slot.grade_coherent) {
                VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
                range.memory=slot.grade_memory;range.size=VK_WHOLE_SIZE;
                VkCheck(vkFlushMappedMemoryRanges(device,1,&range),"Flush grading parameters");
            }
        }
        // Lock in stable address order, avoiding deadlocks when frames are shared.
        std::vector<const AVFrame*> all{src,output.get()};
        if(back && back->data[0]!=src->data[0]) all.push_back(back);
        std::sort(all.begin(),all.end(),[](const AVFrame* a,const AVFrame* b){return std::less<uint8_t*>()(a->data[0],b->data[0]);});
        std::vector<std::unique_ptr<LockedFrame>> locks;
        for(auto* f:all) locks.emplace_back(new LockedFrame(f));
        auto* sf=reinterpret_cast<AVVkFrame*>(src->data[0]);
        auto* of=reinterpret_cast<AVVkFrame*>(output->data[0]);
        const bool separate_nv12 = p.modes[0]==1 && sf->img[1]!=VK_NULL_HANDLE && sf->img[1]!=sf->img[0];
        if(separate_nv12) plane_count=2;
        const auto* svc=reinterpret_cast<const AVVulkanFramesContext*>(src_fc->hwctx);
        auto& owned_views=slot.views;
        VkCommandBuffer cmd=slot.cmd;
        const VkDescriptorSet set=slot.set;
        try {
            // Ordinary outputs use our shared pools. An encoder's pool can
            // disappear when its writer closes, before this cached pipeline.
            slot.target.reset(av_frame_clone(output.get()));
            if(!slot.target) throw std::bad_alloc();
            for(const AVFrame* input_frame : {src, back}) {
                if (!input_frame) continue;
                FramePtr retained(av_frame_clone(input_frame));
                if (!retained) throw std::bad_alloc();
                slot.inputs.push_back(std::move(retained));
            }
            std::array<VkImageView,5> views{};
            auto make_view=[&](VkImage image,VkFormat format,VkImageAspectFlags aspect){
                auto view=View(image,format,aspect);owned_views.push_back(view);return view;
            };
            if(p.modes[0]==1){
                views[0]=make_view(sf->img[0],VK_FORMAT_R8_UNORM,separate_nv12?VK_IMAGE_ASPECT_COLOR_BIT:VK_IMAGE_ASPECT_PLANE_0_BIT);
                views[1]=views[2]=make_view(sf->img[separate_nv12?1:0],VK_FORMAT_R8G8_UNORM,separate_nv12?VK_IMAGE_ASPECT_COLOR_BIT:VK_IMAGE_ASPECT_PLANE_1_BIT);
            }else if(p.modes[0]>=2){
                views[0]=views[1]=views[2]=make_view(sf->img[0],VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT);
            }else{
                for(int i=0;i<3;++i) views[i]=make_view(sf->img[i],svc->format[i],VK_IMAGE_ASPECT_COLOR_BIT);
            }
            views[3]=back?make_view(reinterpret_cast<AVVkFrame*>(back->data[0])->img[0],VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT):views[0];
            if(encode_nv12){
                const bool separate=of->img[1] && of->img[1]!=of->img[0];
                views[3]=make_view(of->img[0],VK_FORMAT_R8_UNORM,separate?VK_IMAGE_ASPECT_COLOR_BIT:VK_IMAGE_ASPECT_PLANE_0_BIT);
                views[4]=make_view(of->img[separate?1:0],VK_FORMAT_R8G8_UNORM,separate?VK_IMAGE_ASPECT_COLOR_BIT:VK_IMAGE_ASPECT_PLANE_1_BIT);
            }else views[4]=make_view(of->img[0],VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_ASPECT_COLOR_BIT);
            std::array<VkDescriptorImageInfo,5> images{};
            std::array<VkWriteDescriptorSet,6> writes{};
            for(int i=0;i<5;++i){
                images[i].sampler=(i==4 || (encode_nv12 && i==3))?VK_NULL_HANDLE:sampler;images[i].imageView=views[i];images[i].imageLayout=VK_IMAGE_LAYOUT_GENERAL;
                writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set;writes[i].dstBinding=i;writes[i].descriptorCount=1;
                writes[i].descriptorType=(i==4 || (encode_nv12 && i==3))?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[i].pImageInfo=&images[i];
            }
            VkDescriptorBufferInfo grade_buffer{slot.grade_buffer,0,sizeof(VulkanGradeParameters)};
            if(!encode_nv12) {
                writes[5].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[5].dstSet=set;writes[5].dstBinding=5;writes[5].descriptorCount=1;
                writes[5].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[5].pBufferInfo=&grade_buffer;
            }
            vkUpdateDescriptorSets(device,encode_nv12?5:6,writes.data(),0,nullptr);
            VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VkCheck(vkBeginCommandBuffer(cmd,&bi),"Begin affine commands");
            std::vector<VkSemaphore> semaphores;
            std::vector<uint64_t> waits,signal_values;
            std::vector<std::pair<AVVkFrame*,int>> changed;
            std::vector<VkImageMemoryBarrier> barriers;
            for(auto* f:all){
                auto* vf=reinterpret_cast<AVVkFrame*>(f->data[0]);
                const int count=f==src?plane_count:(f==output.get() && encode_nv12 && of->img[1] && of->img[1]!=of->img[0]?2:1);
                for(int i=0;i<count;++i){
                    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask=static_cast<VkAccessFlags>(vf->access[i]);
                    barrier.dstAccessMask=f==output.get()?VK_ACCESS_SHADER_WRITE_BIT:VK_ACCESS_SHADER_READ_BIT;
                    barrier.oldLayout=vf->layout[i];barrier.newLayout=VK_IMAGE_LAYOUT_GENERAL;
                    barrier.srcQueueFamilyIndex=vf->queue_family[i];
                    barrier.dstQueueFamilyIndex=vf->queue_family[i]==VK_QUEUE_FAMILY_IGNORED?VK_QUEUE_FAMILY_IGNORED:uint32_t(queue_family);
                    barrier.image=vf->img[i];barrier.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.levelCount=1;barrier.subresourceRange.layerCount=1;
                    barriers.push_back(barrier);changed.emplace_back(vf,i);
                    if(std::find(semaphores.begin(),semaphores.end(),vf->sem[i])==semaphores.end()){
                        semaphores.push_back(vf->sem[i]);waits.push_back(vf->sem_value[i]);signal_values.push_back(vf->sem_value[i]+1);
                    }
                }
            }
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,0,nullptr,barriers.size(),barriers.data());
            vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr);
            vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(p),&p);
            vkCmdDispatch(cmd,(width+15)/16,(height+15)/16,1);
            VkCheck(vkEndCommandBuffer(cmd),"End affine commands");
            VkTimelineSemaphoreSubmitInfo ti{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
            ti.waitSemaphoreValueCount=waits.size();ti.pWaitSemaphoreValues=waits.data();ti.signalSemaphoreValueCount=signal_values.size();ti.pSignalSemaphoreValues=signal_values.data();
            std::vector<VkPipelineStageFlags> stages(waits.size(),VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.pNext=&ti;
            si.waitSemaphoreCount=semaphores.size();si.pWaitSemaphores=semaphores.data();si.pWaitDstStageMask=stages.data();
            si.signalSemaphoreCount=semaphores.size();si.pSignalSemaphores=semaphores.data();si.commandBufferCount=1;si.pCommandBuffers=&cmd;
            vkcontext->lock_queue(device_context,queue_family,0);
            auto result=vkQueueSubmit(queue,1,&si,slot.fence);
            vkcontext->unlock_queue(device_context,queue_family,0);
            VkCheck(result,"Submit affine renderer");slot.busy=true;
            for(size_t changed_index=0;changed_index<changed.size();++changed_index){
                auto item=changed[changed_index];
                item.first->layout[item.second]=VK_IMAGE_LAYOUT_GENERAL;
                item.first->access[item.second]=item.first==of?VK_ACCESS_SHADER_WRITE_BIT:VK_ACCESS_SHADER_READ_BIT;
                item.first->queue_family[item.second]=barriers[changed_index].dstQueueFamilyIndex;
                item.first->sem_value[item.second]++;
            }
            // Publish semaphore values before releasing frame locks. Do not wait here:
            // later readers wait on those semaphores.
            locks.clear();
            return std::make_shared<VulkanFrameImage>(output.get(),!encode_nv12);
        }catch(...){
            // Imported surface destructors acquire the same frame mutexes.
            locks.clear();
            Retire(slot);
            throw;
        }
    }
};
}
static std::shared_ptr<const FrameImage> RenderImage(std::shared_ptr<const FrameImage> source,
    std::shared_ptr<const FrameImage> background,int width,int height,const QTransform& matrix,double opacity,const QColor& clear,bool high_quality,
    const EffectParameters* effect){
    auto src=std::dynamic_pointer_cast<const VulkanFrameImage>(source);
    auto back=std::dynamic_pointer_cast<const VulkanFrameImage>(background);
    if(!src || (background && !back)) throw std::invalid_argument("Vulkan compositor requires Vulkan surfaces");
    if(width<=0 || height<=0) throw std::invalid_argument("Invalid compositor dimensions");
    auto* fc=reinterpret_cast<AVHWFramesContext*>(src->GetFrame()->hw_frames_ctx->data);
    using Key=AVHWDeviceContext*;
    // Output dimensions are push constants, not pipeline state. Animated crop
    // resizing must reuse the pipeline rather than compile one per frame size.
    struct CachedPipeline { std::unique_ptr<Pipeline> pipeline; uint64_t used; };
    thread_local std::map<Key,CachedPipeline> cache;
    thread_local uint64_t clock=0;
    const Key requested=fc->device_ctx;
    auto found=cache.find(requested);
    if(found==cache.end()){
        if(cache.size()>=8){
            auto oldest=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
            cache.erase(oldest);
        }
        found=cache.emplace(requested,CachedPipeline{std::make_unique<Pipeline>(fc->device_ref),0}).first;
    }
    found->second.used=++clock;
    auto& pipeline=found->second.pipeline;
    return pipeline->Render(*src,back.get(),width,height,matrix,opacity,clear,high_quality,nullptr,effect);
}
std::shared_ptr<const FrameImage> RenderVulkanImage(std::shared_ptr<const FrameImage> source,
    std::shared_ptr<const FrameImage> background,int width,int height,const QTransform& matrix,
    double opacity,const QColor& clear,bool high_quality) {
    return RenderImage(source,background,width,height,matrix,opacity,clear,high_quality,nullptr);
}
std::shared_ptr<const FrameImage> SaturateVulkanImage(std::shared_ptr<const FrameImage> source,
    const std::array<float,4>& saturation) {
    const int width=source->GetWidth(),height=source->GetHeight();
    EffectParameters effect{1};
    std::copy(saturation.begin(),saturation.end(),effect.values.begin());
    return RenderImage(source,nullptr,width,height,QTransform(),1.0,Qt::transparent,false,&effect);
}
std::shared_ptr<const FrameImage> CropVulkanImage(std::shared_ptr<const FrameImage> source,
    const QRectF& destination,const QRectF& copy,const QRect& canvas) {
    QTransform matrix;
    matrix.translate(destination.x()-copy.x()-canvas.x(),destination.y()-copy.y()-canvas.y());
    const std::array<float,4> bounds{float(destination.left()-canvas.x()),float(destination.top()-canvas.y()),
        float(destination.right()-canvas.x()),float(destination.bottom()-canvas.y())};
    EffectParameters effect{2};
    std::copy(bounds.begin(),bounds.end(),effect.values.begin());
    return RenderImage(source,nullptr,canvas.width(),canvas.height(),matrix,1.0,Qt::transparent,false,&effect);
}
std::shared_ptr<const FrameImage> GradeVulkanImage(std::shared_ptr<const FrameImage> source,
    const VulkanGradeParameters& parameters) {
    const int width=source->GetWidth(),height=source->GetHeight();
    EffectParameters effect{3,parameters.controls,&parameters};
    return RenderImage(source,nullptr,width,height,QTransform(),1.0,Qt::transparent,false,&effect);
}
std::shared_ptr<const FrameImage> MaskVulkanImage(std::shared_ptr<const FrameImage> source,
    std::shared_ptr<const FrameImage> mask,float contrast,int brightness,bool invert,bool replace) {
    const int width=source->GetWidth(),height=source->GetHeight();
    EffectParameters effect{4,{contrast,float(brightness),float(invert),float(replace)}};
    return RenderImage(source,mask,width,height,QTransform(),1.0,Qt::transparent,false,&effect);
}
std::shared_ptr<const FrameImage> BlurVulkanImage(std::shared_ptr<const FrameImage> source,
    const QRect& area,int horizontal,int vertical,int iterations) {
    const int width=source->GetWidth(),height=source->GetHeight();
    // Blur samples premultiplied RGBA byte values, as the CPU implementation does.
    auto rgba=std::dynamic_pointer_cast<const VulkanFrameImage>(source);
    if(!rgba) throw std::invalid_argument("Blur requires a Vulkan surface");
    const auto* context=reinterpret_cast<const AVHWFramesContext*>(rgba->GetFrame()->hw_frames_ctx->data);
    if(context->sw_format!=AV_PIX_FMT_RGBA || !rgba->IsPremultiplied())
        source=RenderVulkanImage(source,nullptr,width,height,QTransform(),1.0);
    auto result=source;
    for(int i=0;i<iterations;++i) {
        for(int direction=0;direction<2;++direction) {
            const int radius=direction?vertical:horizontal;
            if(radius<=0) continue;
            EffectParameters effect{5,{float(radius),float(direction),float(area.left()),float(area.top()),
                float(area.right()),float(area.bottom())}};
            result=RenderImage(result,nullptr,width,height,QTransform(),1.0,Qt::transparent,false,&effect);
        }
    }
    // QPainter paints the blurred region over the original, including alpha.
    EffectParameters region{2,{float(area.left()),float(area.top()),float(area.right()+1),float(area.bottom()+1)}};
    return RenderImage(result,source,width,height,QTransform(),1.0,Qt::transparent,false,&region);
}
namespace {
// Its ring slots keep recent NV12 frames, which NVENC export maps into CUDA.
thread_local std::unique_ptr<Pipeline> nv12_pipeline;
thread_local AVHWDeviceContext* nv12_key=nullptr;
}
std::shared_ptr<const FrameImage> ConvertVulkanToNV12(std::shared_ptr<const FrameImage> source, const AVFrame* target) {
    auto src=std::dynamic_pointer_cast<const VulkanFrameImage>(source);
    if(!src) throw std::invalid_argument("NV12 conversion requires a Vulkan surface");
    const auto* frame=src->GetFrame();
    if(frame->width%2 || frame->height%2) throw std::invalid_argument("NV12 export requires even dimensions");
    auto* fc=reinterpret_cast<AVHWFramesContext*>(frame->hw_frames_ctx->data);
    const auto requested=fc->device_ctx;
    if(!nv12_pipeline || nv12_key!=requested){
        nv12_pipeline=std::make_unique<Pipeline>(fc->device_ref,true);
        nv12_key=requested;
    }
    return nv12_pipeline->Render(*src,nullptr,frame->width,frame->height,QTransform(),1.0,Qt::black,false,target);
}

uint64_t VulkanPipelineCreationCount() { return pipeline_creations.load(std::memory_order_relaxed); }

void ReleaseVulkanMemory() {
    // Free CUDA-mapped NV12 frames while CUDA is loaded, not at process exit.
    nv12_pipeline.reset();
    ReleaseVulkanUploads();
#ifdef __APPLE__
    ReleaseMacVulkanPools();
#endif
    auto& shared=SharedPools();
    const std::lock_guard<std::mutex> lock(shared.mutex);
    for(auto& item:shared.pools) av_buffer_unref(&item.second.frames);
    shared.pools.clear();
}

}
