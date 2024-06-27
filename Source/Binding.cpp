// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


#include <nosVulkan/Binding.h>

#include <nosVulkan/Image.h>

#include <nosVulkan/Buffer.h>

namespace nos::vk
{

Binding::Binding(rc<Buffer> res, u32 binding, u32 bufferOffset, u32 arrayIdx)
    : Resource(res), Idx(binding), AccessFlags(0), BufferOffset(bufferOffset), ArrayIdx(arrayIdx)
{
}

Binding::Binding(rc<Image> res, u32 binding, VkFilter filter, u32 arrayIdx)
    : Resource(res), Idx(binding), AccessFlags(0), Filter(filter), ArrayIdx(arrayIdx)
{
}

VkFlags Binding::MapTypeToUsage(VkDescriptorType type)
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_IMAGE_USAGE_SAMPLED_BIT;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_IMAGE_USAGE_STORAGE_BIT;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    default:
        return 0;
    }
}

VkImageLayout Binding::MapTypeToLayout(VkDescriptorType type)
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_IMAGE_LAYOUT_GENERAL;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default: return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkAccessFlags Binding::MapTypeToAccess(VkDescriptorType type)
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return  VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    default: return 0;
    }
}


DescriptorResourceInfo Binding::GetDescriptorInfo(VkDescriptorType type) const
{
    VkFlags usage = MapTypeToUsage(type);
    AccessFlags = MapTypeToAccess(type);
    DescriptorResourceInfo Info = {};

    if (auto img = std::get_if<rc<Image>>(&Resource); img && *img)
    {
        Info = (*img)->GetView(usage)->GetDescriptorInfo(Filter);
        Info.Image.imageLayout = MapTypeToLayout(type);
    }
    else if(auto buf = std::get_if<rc<Buffer>>(&Resource); buf && *buf)
    {
        Info = (*buf)->GetDescriptorInfo();
        Info.Buffer.offset = BufferOffset;
        assert(!usage || ((*buf)->Usage & usage));
    }

    return Info;
}
} // namespace nos::vk