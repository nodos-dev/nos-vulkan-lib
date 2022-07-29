#pragma once

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

struct mzVulkan_API ImageView  : private SharedFactory<ImageView>
{
    friend struct Image;

    VkImageView Handle;
    VkFormat Format;
    Sampler Sampler;
    VkImageUsageFlags Usage;
    rc<struct Image> Src;
    ImageView(rc<struct Image> Image, VkFormat Format = VK_FORMAT_UNDEFINED, VkImageUsageFlags Usage = 0);
    ~ImageView();
    DescriptorResourceInfo GetDescriptorInfo() const;

    u64 Hash() const
    {
        return (((u64)Format << 32ull) | (u64)Usage);
    }
};

struct mzVulkan_API Image : SharedFactory<Image>, DeviceChild
{
    Allocation Allocation;
    VkImage Handle;
    VkImageUsageFlags Usage;
    VkExtent2D Extent;
    VkFormat Format;
    ImageState State;
    std::map<u64, rc<ImageView>> Views;

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

    rc<ImageView> GetView(VkFormat Format = VK_FORMAT_UNDEFINED, VkImageUsageFlags Usage = 0)
    { 
        Format = (Format ? Format : this->Format);
        Usage  = (Usage ? Usage : this->Usage);
        const u64 hash = (((u64)Format << 32ull) | (u64)Usage);
        auto it = Views.find(hash);
        if (it != Views.end())
        {
            return it->second;
        }
        return Views[hash] = ImageView::New(shared_from_this(), Format, Usage);
    }

    rc<ImageView> GetView(VkFormat fmt) 
    { 
        return GetView(fmt, Usage); 
    }

    rc<ImageView> GetView(VkImageUsageFlags usage)
    {
        return GetView(Format, usage); 
    }
};

}; // namespace mz::vk