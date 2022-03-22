#pragma once

#include "mzVkCommon.h"

namespace mz::vk
{

struct ImageExportInfo
{
    HANDLE memory = 0;
    HANDLE sync   = 0;

    VkDeviceSize offset = 0;
    VkDeviceSize size   = 0;

    VkAccessFlags accessMask = 0;
};

struct ImageCreateInfo
{
    VkExtent2D             Extent;
    VkFormat               Format;
    VkImageUsageFlags      Usage;
    const ImageExportInfo* Exported;
};

}; // namespace mz::vk