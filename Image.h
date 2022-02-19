#pragma once

#include "Buffer.h"

#include "Command.h"

struct ImageCreateInfo
{
    VkExtent2D        Extent;
    VkImageLayout     FinalLayout;
    VkFormat          Format;
    VkImageUsageFlags Usage;
    u32               MipLevels;
};

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

    HANDLE GetOSHandle()
    {
        return Allocation.GetOSHandle();
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

    void Upload(u64 sz, u8* data, VulkanAllocator* = 0, CommandPool* = 0) ;

    std::shared_ptr<VulkanBuffer> Download(VulkanAllocator* = 0, CommandPool* = 0);

    VulkanImage(VulkanAllocator* Allocator, ImageCreateInfo const& createInfo, HANDLE OSHandle = 0);

    VulkanImage(VulkanDevice* Vk, ImageCreateInfo const& createInfo, HANDLE OSHandle = 0);
};