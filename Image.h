#pragma once

#include "Allocator.h"
#include "InfoStructs.h"

#include "Buffer.h"

#include "Command.h"

namespace mz
{

void ImageLayoutTransition(VkImage                        Image,
                           std::shared_ptr<CommandBuffer> Cmd,
                           VkImageLayout                  CurrentLayout,
                           VkImageLayout                  TargetLayout,
                           VkAccessFlags                  srcAccessMask,
                           VkAccessFlags                  dstAccessMask);

struct VulkanImage : std::enable_shared_from_this<VulkanImage>
{
    VulkanDevice* Vk;

    Allocation Allocation;

    VkImage           Handle;
    VkExtent2D        Extent;
    VkFormat          Format;
    VkImageUsageFlags Usage;

    VkSampler   Sampler;
    VkImageView View;

    VkImageLayout Layout;
    VkAccessFlags AccessMask;

    HANDLE      Sync;
    VkSemaphore Sema;

    ImageExportInfo
    GetExportInfo()
    {
        return ImageExportInfo{
            .memory     = Allocation.GetOSHandle(),
            .sync       = Sync,
            .offset     = Allocation.Offset + Allocation.Block->Offset,
            .size       = Allocation.Size,
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

    ~VulkanImage();

    void Transition(std::shared_ptr<CommandBuffer> cmd, VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);
    void Transition(VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);

    void Upload(u8* data, VulkanAllocator* = 0, CommandPool* = 0);

    std::shared_ptr<VulkanImage> Copy(VulkanAllocator* = 0, CommandPool* = 0);

    std::shared_ptr<VulkanBuffer> Download(VulkanAllocator* = 0, CommandPool* = 0);

    VulkanImage(VulkanAllocator*, ImageCreateInfo const&);

    VulkanImage(VulkanDevice*, ImageCreateInfo const&);

}; // namespace mz
}; // namespace mz