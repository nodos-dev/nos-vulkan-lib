#pragma once

#include "vulkan/vulkan_core.h"
#include <Buffer.h>

#include <Command.h>

namespace mz::vk
{

struct mzVulkan_API Semaphore
{
    VkSemaphore Handle;
    Device* Vk;

    Semaphore(Device* Vk, const MemoryExportInfo* Imported);
    Semaphore(Device* Vk, u64 pid, HANDLE ext);
    HANDLE GetSyncOSHandle() const;

    operator VkSemaphore() const
    {
        return Handle;
    }
};

struct mzVulkan_API Image : SharedFactory<Image>
{
    Device* Vk;

    Allocation Allocation;

    VkImage Handle;
    VkExtent2D Extent;
    VkFormat Format;
    VkImageUsageFlags Usage;

    VkImageLayout Layout;
    VkAccessFlags AccessMask;

    Semaphore Sema;

    VkSampler Sampler;
    VkImageView View;

    MemoryExportInfo GetExportInfo() const;

    DescriptorResourceInfo GetDescriptorInfo() const;

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