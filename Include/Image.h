#pragma once

#include <Allocator.h>

namespace mz::vk
{

struct Buffer;
struct Allocation;

struct mzVulkan_API Image : SharedFactory<Image>, DeviceChild
{
    Allocation Allocation;
    VkImage Handle;
    VkImageView View;
    VkSampler Sampler;
    VkImageUsageFlags Usage;
    VkExtent2D Extent;
    VkFormat Format;
    ImageState State;

    DescriptorResourceInfo GetDescriptorInfo() const;
    MemoryExportInfo GetExportInfo() const;
    void Transition(rc<CommandBuffer> Cmd, ImageState Dst);
    void BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src);
    void Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src);
    rc<Image> Copy(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    rc<Buffer> Download(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    Image(Allocator*, ImageCreateInfo const&);
    Image(Device* Vk, ImageCreateInfo const& createInfo);
    ~Image();
};

}; // namespace mz::vk