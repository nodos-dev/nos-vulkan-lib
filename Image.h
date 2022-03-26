#pragma once

#include <Buffer.h>

#include <Command.h>

namespace mz::vk
{


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

    HANDLE Sync;
    VkSemaphore Sema;

    VkSampler Sampler;
    VkImageView View;

    ImageExportInfo GetExportInfo() const;

    DescriptorResourceInfo GetDescriptorInfo() const;


    void Transition(std::shared_ptr<CommandBuffer> cmd, VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);
    void Transition(VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask);

    void Upload(u8* data, Allocator* = 0, CommandPool* = 0);
    void Upload(std::shared_ptr<Buffer>, CommandPool* = 0);

    std::shared_ptr<Image> Copy(Allocator* = 0, CommandPool* = 0);

    std::shared_ptr<Buffer> Download(Allocator* = 0, CommandPool* = 0);

    Image(Allocator*, ImageCreateInfo const&);

    Image(Device* Vk, ImageCreateInfo const& createInfo);
    
    ~Image();
};

}; // namespace mz::vk