#pragma once

#include "mzVkCommon.h"

namespace mz
{

struct ImageExportInfo
{
    HANDLE sync;
    HANDLE memory;

    VkDeviceSize offset;
    VkDeviceSize size;
};

struct ImageCreateInfo
{
    VkExtent2D        Extent;
    VkFormat          Format;
    VkImageUsageFlags Usage;
    ImageExportInfo   Ext;
};

}; // namespace mz