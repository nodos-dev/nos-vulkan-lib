#pragma once

#include <Allocator.h>

namespace mz::vk
{

struct Buffer;
struct Allocation;

struct mzVulkan_API Semaphore
{
    VkSemaphore Handle;
    HANDLE OSHandle;
    Semaphore(Device* Vk, const MemoryExportInfo* Imported);
    Semaphore(Device* Vk, u64 pid, HANDLE ext);
    operator VkSemaphore() const;
    void Free(Device* Vk);
};

struct mzVulkan_API Image : SharedFactory<Image>
{
    Device* Vk;
    Allocation Allocation;
    VkImage Handle;
    VkImageView View;
    VkSampler Sampler;

    Semaphore Sema;

    VkExtent2D Extent;
    VkFormat Format;
    VkImageUsageFlags Usage;
    VkImageLayout Layout;
    VkAccessFlags AccessMask;

    DescriptorResourceInfo GetDescriptorInfo() const;
    MemoryExportInfo GetExportInfo() const;
    rc<CommandBuffer> Transition(rc<CommandBuffer> Cmd, VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);
    rc<CommandBuffer> BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src);
    rc<CommandBuffer> Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src);
    rc<Image> Copy(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    rc<Buffer> Download(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    Image(Allocator*, ImageCreateInfo const&);
    Image(Device* Vk, ImageCreateInfo const& createInfo);
    ~Image();
};

}; // namespace mz::vk