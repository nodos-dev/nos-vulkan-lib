#pragma once

#include <mzVkCommon.h>

namespace mz::vk
{

struct Image;
struct Buffer;

template <class T>
concept TypeClassResource = std::same_as<T, std::shared_ptr<Image>> || std::same_as<T, std::shared_ptr<Buffer>>;

struct mzVulkan_API Binding
{
    using Type = std::variant<Buffer*, Image*>;

    Type resource;

    u32 binding;

    std::shared_ptr<DescriptorResourceInfo> info;

    VkAccessFlags access;

    auto operator<=>(const Binding& other) const
    {
        return binding <=> other.binding;
    }

    Binding() = default;

    Binding(std::shared_ptr<Buffer> res, u32 binding);

    Binding(std::shared_ptr<Image> res, u32 binding);

    void SanityCheck(VkDescriptorType type);

    VkWriteDescriptorSet GetDescriptorInfo(VkDescriptorSet set, VkDescriptorType type);
};

} // namespace mz::vk