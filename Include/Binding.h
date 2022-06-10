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

    Type Resource;
    u32 Idx;
    u32 BufferOffset;
    mutable VkAccessFlags AccessFlags;
    Binding(Type res, u32 binding, u32 bufferOffset = 0);

    DescriptorResourceInfo GetDescriptorInfo(VkDescriptorType type) const;
};

} // namespace mz::vk