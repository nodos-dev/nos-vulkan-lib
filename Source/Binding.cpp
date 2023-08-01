// Copyright MediaZ AS. All Rights Reserved.


#include <mzVulkan/Binding.h>

#include <mzVulkan/Image.h>

#include <mzVulkan/Buffer.h>

namespace mz::vk
{

Binding::Binding(rc<Buffer> res, u32 binding, u32 bufferOffset)
    : Resource(res), Idx(binding), AccessFlags(0), BufferOffset(bufferOffset)
{
}

Binding::Binding(rc<Image> res, u32 binding, VkFilter filter)
    : Resource(res), Idx(binding), AccessFlags(0), Filter(filter)
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

    if (auto img = std::get_if<rc<Image>>(&Resource))
    {
        Info = (*img)->GetView(usage)->GetDescriptorInfo(Filter);
        Info.Image.imageLayout = MapTypeToLayout(type);
    }
    else 
    {
        auto buf = std::get<rc<Buffer>>(Resource);
        Info = buf->GetDescriptorInfo();
        Info.Buffer.offset = BufferOffset;
        assert(!usage || (buf->Usage & usage));
    }

    return Info;
}
} // namespace mz::vk