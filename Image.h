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

    void Transition(rc<CommandBuffer> cmd, VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);
    void Transition(VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);

    void Upload(u8* data, Allocator* = 0, CommandPool* = 0);
    void Upload(rc<Buffer>, CommandPool* = 0);

    rc<Image> Copy(Allocator* = 0, CommandPool* = 0);

    rc<Buffer> Download(Allocator* = 0, CommandPool* = 0);

    Image(Allocator*, ImageCreateInfo const&);

    Image(Device* Vk, ImageCreateInfo const& createInfo);

    ~Image();

    void BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Img)
    {
        Img->Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
        Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkImageBlit blit = {
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .srcOffsets     = {{}, {(i32)Img->Extent.width, (i32)Img->Extent.height, 1}},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .dstOffsets = {{}, {(i32)Extent.width, (i32)Extent.height, 1}},
        };

        Cmd->BlitImage(Img->Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    }
};

}; // namespace mz::vk