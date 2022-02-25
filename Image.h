#pragma once

#include "Allocator.h"
#include "InfoStructs.h"

#include "Buffer.h"

#include "Command.h"
#include "vulkan/vulkan_core.h"

namespace mz
{

void ImageLayoutTransition(VkImage                        Image,
                           std::shared_ptr<CommandBuffer> Cmd,
                           VkImageLayout                  CurrentLayout,
                           VkImageLayout                  TargetLayout,
                           VkAccessFlags                  srcAccessMask,
                           VkAccessFlags                  dstAccessMask,
                           u32                            srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                           u32                            dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

struct VulkanImage : SharedFactory<VulkanImage>
{
    VulkanDevice* Vk;

    Allocation Allocation;

    VkImage           Handle;
    VkExtent2D        Extent;
    VkFormat          Format;
    VkImageUsageFlags Usage;

    VkImageLayout Layout;
    VkAccessFlags AccessMask;

    HANDLE      Sync;
    VkSemaphore Sema;

    VkSampler   Sampler;
    VkImageView View;

    ImageExportInfo
    GetExportInfo()
    {
        return ImageExportInfo{
            .memory     = Allocation.GetOSHandle(),
            .sync       = Sync,
            .offset     = Allocation.Offset + Allocation.Block->Offset,
            .size       = Allocation.Block->Size,
            .accessMask = AccessMask,
        };
    }

    DescriptorResourceInfo GetDescriptorInfo() const
    {
        return DescriptorResourceInfo{
            .image = {
                .sampler     = Sampler,
                .imageView   = View,
                .imageLayout = Layout,
            }};
    }

    void Transition(std::shared_ptr<CommandBuffer> cmd, VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);
    void Transition(VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);

    void Upload(u8* data, VulkanAllocator* = 0, CommandPool* = 0);
    void Upload(std::shared_ptr<VulkanBuffer>, CommandPool* = 0);

    std::shared_ptr<VulkanImage> Copy(VulkanAllocator* = 0, CommandPool* = 0);

    std::shared_ptr<VulkanBuffer> Download(VulkanAllocator* = 0, CommandPool* = 0);

    VulkanImage(VulkanAllocator*, ImageCreateInfo const&);

    VulkanImage(VulkanDevice* Vk, ImageCreateInfo const& createInfo)
        : VulkanImage(Vk->ImmAllocator.get(), createInfo)
    {
    }

    ~VulkanImage()
    {
        if (!Allocation.IsImported())
        {
            assert(SUCCEEDED(CloseHandle(Sync)));
        }
        Vk->DestroySemaphore(Sema, 0);
        Vk->DestroyImage(Handle, 0);
        Vk->DestroyImageView(View, 0);
        Vk->DestroySampler(Sampler, 0);
        Allocation.Free();
    };
};

}; // namespace mz