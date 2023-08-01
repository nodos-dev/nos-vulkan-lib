/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Common.h"

namespace mz::vk
{

struct Image;
struct Buffer;

template <class T>
concept TypeClassResource = std::same_as<T, rc<Image>> || std::same_as<T, rc<Buffer>>;

struct mzVulkan_API Binding
{
    static VkFlags MapTypeToUsage(VkDescriptorType type);
    static VkImageLayout MapTypeToLayout(VkDescriptorType type);
    static VkAccessFlags MapTypeToAccess(VkDescriptorType type);
    using Type = std::variant<rc<Image>, rc<Buffer>>;

    Type Resource;
    u32 Idx;
    mutable VkAccessFlags AccessFlags;
    union
    {
        u32 BufferOffset;
        VkFilter Filter;
    };
    Binding() = delete;
    Binding(rc<Buffer> res, u32 binding, u32 bufferOffset);
    Binding(rc<Image> res, u32 binding, VkFilter filter);

    DescriptorResourceInfo GetDescriptorInfo(VkDescriptorType type) const;
};

} // namespace mz::vk