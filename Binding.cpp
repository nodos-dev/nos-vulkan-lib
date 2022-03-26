
#include <Binding.h>

#include <Image.h>

namespace mz::vk
{

Binding::Binding(std::shared_ptr<Buffer> res, u32 binding)
    : resource(res.get()), binding(binding), info(new DescriptorResourceInfo(res->GetDescriptorInfo())), access(0)
{
}

Binding::Binding(std::shared_ptr<Image> res, u32 binding)
    : resource(res.get()), binding(binding), info(new DescriptorResourceInfo(res->GetDescriptorInfo())), access(0)
{
}

void Binding::SanityCheck(VkDescriptorType type)
{
    VkFlags Usage = 0;

    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        assert(std::holds_alternative<Image*>(resource));
        Usage = std::get<Image*>(resource)->Usage;
        break;
    default:
        assert(std::holds_alternative<Buffer*>(resource));
        Usage = std::get<Buffer*>(resource)->Usage;
        break;
    }

    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        assert(Usage & VK_IMAGE_USAGE_SAMPLED_BIT);
        info->image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        access                  = VK_ACCESS_SHADER_READ_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        assert(Usage & VK_IMAGE_USAGE_STORAGE_BIT);
        info->image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        access                  = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        assert(Usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        info->image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        access                  = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        assert(Usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        assert(Usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
        assert(Usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        assert(Usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        break;
    default:
        assert(0);
        break;
    }
}

VkWriteDescriptorSet Binding::GetDescriptorInfo(VkDescriptorSet set, VkDescriptorType type)
{
    SanityCheck(type);

    return VkWriteDescriptorSet{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = set,
        .dstBinding      = binding,
        .descriptorCount = 1,
        .descriptorType  = type,
        .pImageInfo      = (std::holds_alternative<Image*>(resource) ? (&info->image) : 0),
        .pBufferInfo     = (std::holds_alternative<Buffer*>(resource) ? (&info->buffer) : 0),
    };
}
} // namespace mz::vk