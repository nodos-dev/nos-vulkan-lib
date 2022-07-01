#pragma once

#include "vulkan/vulkan_core.h"
#include <Allocator.h>

namespace mz::vk
{

struct CommandBuffer;
struct Buffer;
struct Allocation;


struct mzVulkan_API Sampler 
{
    VkSampler Handle;
    Sampler() = default;
    Sampler(Device* Vk, VkSamplerYcbcrConversion SamplerYcbcrConversion);
    void Free(Device* Vk);

    operator VkSampler() const { return Handle; }
};

struct mzVulkan_API Image : SharedFactory<Image>, DeviceChild
{
    Allocation Allocation;
    VkImage Handle;
    VkImageView View;
    Sampler Sampler;
    VkSamplerYcbcrConversion SamplerYcbcrConversion;
    VkImageUsageFlags Usage;
    VkExtent2D Extent;
    VkFormat Format;
    ImageState State;

    DescriptorResourceInfo GetDescriptorInfo() const;
    MemoryExportInfo GetExportInfo() const;
    void Transition(rc<CommandBuffer> Cmd, ImageState Dst);
    void BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src);
    void CopyFrom(rc<CommandBuffer> Cmd, rc<Image> Src);
    void ResolveFrom(rc<CommandBuffer> Cmd, rc<Image> Src);

    void Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src, u32 bufferRowLength = 0, u32 bufferImageHeight = 0);
    rc<Image> Copy(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    rc<Buffer> Download(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    Image(Allocator*, ImageCreateInfo const&);
    Image(Device* Vk, ImageCreateInfo const& createInfo);
    ~Image();
};

}; // namespace mz::vk