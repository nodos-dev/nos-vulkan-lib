/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
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

struct ImageCreationInfos
{
    uint32_t MemoryTypeIndex = UINT32_MAX;
    uint32_t ExtMemHandleType = 0;
    VmaAllocationCreateInfo AllocCreateInfo{};
    VkImageCreateInfo ImgCreateInfo{};
    VkExternalMemoryImageCreateInfo ExtMemCreateInfo{};
    VkMemoryPropertyFlags MemProps = 0;
    ImageCreationInfos(ImageCreationInfos&& o) noexcept;
    ImageCreationInfos(ImageCreationInfos const&) = delete;
    ImageCreationInfos() = default;
    ImageCreationInfos& operator=(const ImageCreationInfos&) = delete;
};
    
Result<ImageCreationInfos> nosVulkan_API CalculateImageCreationInfos(vk::Device* device, ImageCreateRequest const& info);

ImageCreateRequest nosVulkan_API GetTempImageCreateRequest(VkExtent3D extent, VkFormat format);
    
struct nosVulkan_API Image : SharedFactory<Image>, ResourceBase<VkImage>
{
protected:
    const VkExtent3D Extent = {};
    const VkFormat Format = VK_FORMAT_UNDEFINED;
    Image(Device* Vk,
		  VkImage img,
		  VkExtent3D extent,
		  VkFormat format,
		  VkImageUsageFlags usage,
		  ImageState state,
          std::optional<Allocation> allocation,
		  VkDeviceSize size,
		  VkImageType imageType);

public:
    static Result<rc<Image>> Create(Device* Vk, ImageCreateRequest const& createInfo, VkResult* outVkRes = nullptr);
    static rc<Image> FromExisting(Device* Vk, VkImage img, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, ImageState state, std::optional<Allocation> allocation, VkDeviceSize size, VkImageType imageType);
    static Result<ImageCreateRequest> TryGetRelaxedSuitableCreateRequest(Device* Vk, ImageCreateRequest const& info);
    static Result<rc<Image>> CreateRelaxed(Device* Vk, ImageCreateRequest const& createInfo, VkResult* vkRes = nullptr);
	vk::Image* AsImage() override { return this; }
    const VkImageUsageFlags Usage = 0;
	const VkImageType ImageType = VK_IMAGE_TYPE_2D;

    ImageState State = {}; // This is not thread safe.
    std::map<u64, rc<ImageView>> Views;

    void Transition(rc<CommandBuffer> curCmd, ImageState Dst);
    void BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src, VkFilter Filter, std::optional<std::vector<VkImageCopy>> regions = std::nullopt);
    void CopyFrom(rc<CommandBuffer> Cmd, rc<Image> Src, std::optional<std::vector<VkImageCopy>> regions = std::nullopt);
    void ResolveFrom(rc<CommandBuffer> Cmd, rc<Image> Src);

    VkExtent3D GetEffectiveExtent() const;
    VkFormat GetEffectiveFormat() const;
    VkFormat GetFormat() const { return Format; }
    VkExtent3D GetExtent() const { return Extent; }

    rc<Image> Copy(rc<CommandBuffer> Cmd);
    void CopyFromBuffer(rc<CommandBuffer> Cmd, rc<Buffer> Src, std::optional<std::vector<VkBufferImageCopy>> regions = std::nullopt);
    void CopyToBuffer(rc<CommandBuffer> Cmd, rc<Buffer>, std::optional<std::vector<VkBufferImageCopy>> regions = std::nullopt);
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
};

}; // namespace nos::vk