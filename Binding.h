#pragma once

#include <mzVkCommon.h>

namespace mz::vk
{

struct Image;
struct Buffer;

template <class T>
concept TypeClassResource = std::same_as<T, rc<Image>> || std::same_as<T, rc<Buffer>>;

struct mzVulkan_API Binding
{
    using Type = std::variant<rc<Image>, rc<Buffer>>;

    Type resource;

    u32 binding;

    rc<DescriptorResourceInfo> info;

    VkAccessFlags access;

    auto operator<=>(const Binding& other) const
    {
        return binding <=> other.binding;
    }

    Binding() = default;

    Binding(Type res, u32 binding);

    void SanityCheck(VkDescriptorType type);

    VkWriteDescriptorSet GetDescriptorInfo(VkDescriptorSet set, VkDescriptorType type);
};

} // namespace mz::vk