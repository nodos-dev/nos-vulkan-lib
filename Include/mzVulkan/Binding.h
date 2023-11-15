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
    u32 Idx = 0;
    u32 ArrayIdx = 0;
    mutable VkAccessFlags AccessFlags;
    union
    {
        u32 BufferOffset;
        VkFilter Filter;
    };
    Binding() = default;
    Binding(rc<Buffer> res, u32 binding, u32 bufferOffset, u32 arrayIdx);
    Binding(rc<Image> res,  u32 binding, VkFilter filter, u32 arrayIdx);

    DescriptorResourceInfo GetDescriptorInfo(VkDescriptorType type) const;

    bool operator < (Binding const& r) const
    {
        if(Idx == r.Idx) return ArrayIdx < r.ArrayIdx;
        return Idx < r.Idx;
    }
    bool operator == (Binding const& r) const
    {
        return Idx == r.Idx && ArrayIdx == r.ArrayIdx;
    }
};

} // namespace mz::vk