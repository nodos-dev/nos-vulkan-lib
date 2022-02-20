#pragma once

#include "mzVkCommon.h"
#include "vulkan/vulkan_core.h"

namespace mz
{

struct ExtHandle
{
    HANDLE sync;
    HANDLE memory;
};

struct ImageCreateInfo
{
    VkExtent2D        Extent;
    VkFormat          Format;
    VkImageUsageFlags Usage;
    u32               MipLevels;
    ExtHandle         Ext;
};

}; // namespace mz