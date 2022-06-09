#pragma once

#include <Device.h>

namespace mz::vk
{

struct mzVulkan_API NativeAPI
{
    Device* Vk;

    NativeAPI(Device* Vk)
        : Vk(Vk)
    {
    }

    virtual void* CreateSharedMemory(u64) = 0;
    virtual void* CreateSharedSync()      = 0;
    virtual void* CreateSharedTexture(VkExtent2D extent, VkFormat format) = 0;
};

} // namespace mz::vk