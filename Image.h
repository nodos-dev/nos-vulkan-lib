#pragma once

#include "InfoStructs.h"

#include "Buffer.h"

#include "Command.h"

namespace mz
{

void ImageLayoutTransition(VkImage                        Image,
                           std::shared_ptr<CommandBuffer> Cmd,
                           VkImageLayout                  CurrentLayout,
                           VkImageLayout                  TargetLayout);

struct VulkanImage : std::enable_shared_from_this<VulkanImage>
{
    VulkanDevice* Vk;

    Allocation Allocation;

    VkImage           Handle;
    VkExtent2D        Extent;
    VkFormat          Format;
    VkImageUsageFlags Usage;
    u32               MipLevels;

    VkImageLayout FinalLayout;

    VkSampler     Sampler;
    VkImageView   View;
    VkImageLayout Layout;

    HANDLE      Sync;
    VkSemaphore Sema;
    VkFence     Fence;

    ExtHandle GetOSHandle()
    {
        return {Allocation.GetOSHandle(), Sync};
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

    void Transition(std::shared_ptr<CommandBuffer> cmd, VkImageLayout TargetLayout);

    void Upload(u64 sz, u8* data, VulkanAllocator* = 0, CommandPool* = 0);

    std::shared_ptr<VulkanBuffer> Download(VulkanAllocator* = 0, CommandPool* = 0);

    VulkanImage(VulkanAllocator*, ImageCreateInfo const&);

    VulkanImage(VulkanDevice*, ImageCreateInfo const&);
};
}; // namespace mz