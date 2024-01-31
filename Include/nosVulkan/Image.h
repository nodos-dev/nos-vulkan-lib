/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Allocation.h"
#include "Semaphore.h"



namespace nos::vk
{

struct CommandBuffer;
struct Buffer;
struct Allocation;

struct nosVulkan_API ImageView  : SharedFactory<ImageView>, DeviceChild
{
    friend struct Image;
    VkImageView Handle;
private:
    VkFormat Format;
public:
    VkImageUsageFlags Usage;
    struct Image* Src;
    ImageView(struct Image* Image, VkFormat Format = VK_FORMAT_UNDEFINED, VkImageUsageFlags Usage = 0);
    ~ImageView();
    DescriptorResourceInfo GetDescriptorInfo(VkFilter) const;

    u64 Hash() const
    {
        return (((u64)Format << 32ull) | (u64)Usage);
    }

    VkFormat GetEffectiveFormat() const { return IsYCbCr(Format) ? VK_FORMAT_R8G8B8A8_UNORM : Format; }
    VkFormat GetFormat() const { return Format; }
};

struct nosVulkan_API Image : SharedFactory<Image>, ResourceBase<VkImage>
{
private:
    VkExtent2D Extent = {0, 0};
    VkFormat Format = VK_FORMAT_UNDEFINED;
public:
	vk::Image* AsImage() override { return this; }
    VkImageUsageFlags Usage = 0;

    ImageState State = {}; // This is not thread safe.
    std::map<u64, rc<ImageView>> Views;
	rc<vk::Semaphore> ExtSemaphore;
	bool Owned = true;

    Image(Device* Vk, ImageCreateInfo const& createInfo, VkResult* re = 0);
	Image(Device* Vk, VkImage img);

    void Transition(rc<CommandBuffer> Cmd, ImageState Dst);
    void BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src, VkFilter Filter);
    void CopyFrom(rc<CommandBuffer> Cmd, rc<Image> Src);
    void ResolveFrom(rc<CommandBuffer> Cmd, rc<Image> Src);

    VkExtent2D GetEffectiveExtent() const { return { Extent.width / (1 + IsYCbCr(Format)), Extent.height}; }
    VkFormat GetEffectiveFormat() const { return IsYCbCr(Format) ? VK_FORMAT_R8G8B8A8_UNORM : Format; }
    VkFormat GetFormat() const { return Format; }
    VkExtent2D GetExtent() const { return Extent; }

    void Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src, u32 bufferRowLength = 0, u32 bufferImageHeight = 0);
    rc<Image> Copy(rc<CommandBuffer> Cmd);
    rc<Buffer> Download(rc<CommandBuffer> Cmd);
    void Download(rc<CommandBuffer> Cmd, rc<Buffer>);
    void Clear(rc<CommandBuffer> Cmd, VkClearColorValue value);

    ~Image();

    rc<ImageView> GetView(VkFormat Format = VK_FORMAT_UNDEFINED, VkImageUsageFlags Usage = 0);
    rc<ImageView> GetView(VkFormat fmt) 
    { 
        return GetView(fmt, Usage); 
    }

    bool IsValid() const { return Handle; }
    rc<ImageView> GetView(VkImageUsageFlags usage)
    {
        return GetView(Format, usage); 
    }

    VkImageAspectFlags GetAspect() const
    {
        return (Format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageType GetImageType() const
    {
        return VK_IMAGE_TYPE_2D; // Temporary fix for color nodes.
        return (1 >= Extent.height) ? VK_IMAGE_TYPE_1D : VK_IMAGE_TYPE_2D;
    }
};

}; // namespace nos::vk