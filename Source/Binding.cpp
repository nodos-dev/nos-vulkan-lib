// Copyright MediaZ AS. All Rights Reserved.


#include <mzVulkan/Binding.h>

#include <mzVulkan/Image.h>

#include <mzVulkan/Buffer.h>

namespace mz::vk
{

Binding::Binding(Type res, u32 binding, u32 bufferOffset)
    : Resource(res), Idx(binding), AccessFlags(0), BufferOffset(bufferOffset)
{
}

DescriptorResourceInfo Binding::GetDescriptorInfo(VkDescriptorType type) const
{
    DescriptorResourceInfo Info = std::visit([](auto res) { return res->GetDescriptorInfo(); }, Resource);
    if (BufferOffset)
    {
        assert(std::holds_alternative<rc<Buffer>>(Resource));
        Info.Buffer.offset = BufferOffset;
    }

    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        assert(std::get<rc<ImageView>>(Resource)->Usage & VK_IMAGE_USAGE_SAMPLED_BIT);
        Info.Image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        AccessFlags            = VK_ACCESS_SHADER_READ_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        assert(std::get<rc<ImageView>>(Resource)->Usage & VK_IMAGE_USAGE_STORAGE_BIT);
        Info.Image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        AccessFlags            = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        assert(std::get<rc<ImageView>>(Resource)->Usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        Info.Image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        AccessFlags            = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        assert(std::get<rc<Buffer>>(Resource)->Usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        assert(std::get<rc<Buffer>>(Resource)->Usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
        assert(std::get<rc<Buffer>>(Resource)->Usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        assert(std::get<rc<Buffer>>(Resource)->Usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        break;
    default:
        assert(0);
        break;
    }
    return Info;
}
} // namespace mz::vk