#pragma once

#include "mzVkCommon.h"
#include "vulkan/vulkan_core.h"

namespace mz
{

struct ExtHandle
{
    HANDLE memory;
    HANDLE sync;
};

struct ImageCreateInfo
{
    VkExtent2D        Extent;
    VkImageLayout     FinalLayout;
    VkFormat          Format;
    VkImageUsageFlags Usage;
    u32               MipLevels;
    ExtHandle         Ext;
};

}; // namespace mz