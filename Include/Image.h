#pragma once

#include "Image.h"
#include <Allocator.h>

namespace mz::vk
{

struct CommandBuffer;
struct Buffer;
struct Allocation;


struct mzVulkan_API Sampler 
{
    VkSampler Handle = 0;
    VkSamplerYcbcrConversion SamplerYcbcrConversion = 0;
    Sampler() = default;
    Sampler(Device* Vk, VkFormat Format);
    operator VkSampler() const { return Handle; }
};

struct mzVulkan_API ImageView  : SharedFactory<ImageView>
{
    VkImageView Handle;
    VkFormat Format;
    Sampler Sampler;
    VkImageUsageFlags Usage;
    rc<struct Image> Src;
    ImageView(rc<struct Image> Image, VkFormat Format = VK_FORMAT_UNDEFINED, VkComponentMapping Components = {}, VkImageUsageFlags Usage = 0);
    ~ImageView();
    DescriptorResourceInfo GetDescriptorInfo() const;
};

struct mzVulkan_API Image : SharedFactory<Image>, DeviceChild
{
    Allocation Allocation;
    VkImage Handle;
    VkImageUsageFlags Usage;
    VkExtent2D Extent;
    VkFormat Format;
    ImageState State;

    MemoryExportInfo GetExportInfo() const;
    void Transition(rc<CommandBuffer> Cmd, ImageState Dst);
    void BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src);
    void CopyFrom(rc<CommandBuffer> Cmd, rc<Image> Src);
    void ResolveFrom(rc<CommandBuffer> Cmd, rc<Image> Src);

    void Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src, u32 bufferRowLength = 0, u32 bufferImageHeight = 0);
    rc<Image> Copy(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    rc<Buffer> Download(rc<CommandBuffer> Cmd, rc<Allocator> Allocator = 0);
    void Download(rc<CommandBuffer> Cmd, rc<Buffer>);
    Image(Allocator*, ImageCreateInfo const&);
    Image(Device* Vk, ImageCreateInfo const& createInfo);
    ~Image();

    rc<ImageView> GetView() 
    { 
        return ImageView::New(shared_from_this()); 
    }

    rc<ImageView> GetView(VkFormat fmt) 
    { 
        return ImageView::New(shared_from_this(), fmt); 
    }

    rc<ImageView> GetView(VkComponentMapping comp) 
    { 
        return ImageView::New(shared_from_this(), Format, comp); 
    }

    rc<ImageView> GetView(VkImageUsageFlags usage)
    {
        return ImageView::New(shared_from_this(), Format, VkComponentMapping{}, usage);
    }
};

}; // namespace mz::vk