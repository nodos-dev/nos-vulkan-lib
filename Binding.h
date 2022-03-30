#pragma once

#include <mzVkCommon.h>

namespace mz::vk
{

struct Image;
struct Buffer;

template <class T>
concept TypeClassResource = std::same_as<T, rc<Image>> || std::same_as<T, rc<Buffer>>;

struct mzVulkan_API Binding: SharedFactory<Binding>
{
    using Type = std::variant<rc<Image>, rc<Buffer>>;

    Type Resource;

    u32 Idx;

    DescriptorResourceInfo Info;

    VkAccessFlags AccessFlags;

    auto operator<=>(const Binding& other) const
    {
        return Idx <=> other.Idx;
    }

    Binding(Type res, u32 binding, u32 bufferOffset = 0);

    void SanityCheck(VkDescriptorType type);

    VkWriteDescriptorSet GetDescriptorInfo(VkDescriptorSet set, VkDescriptorType type);
};

} // namespace mz::vk