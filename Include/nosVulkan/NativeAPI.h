/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Device.h"

namespace nos::vk
{

struct nosVulkan_API NativeAPI : DeviceChild
{
    NativeAPI(Device* Vk)
        : DeviceChild(Vk)
    {
    }

    virtual void* CreateSharedMemory(u64) = 0;
    virtual void* CreateSharedSync()      = 0;
    virtual void* CreateSharedTexture(VkExtent2D extent, VkFormat format) = 0;
};

} // namespace nos::vk