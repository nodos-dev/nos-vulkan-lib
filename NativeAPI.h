#pragma once

#include <mzCommon.h>

#include "InfoStructs.h"

namespace mz
{

struct NativeAPI
{
    struct VulkanDevice* Vk;

    NativeAPI(VulkanDevice* Vk)
        : Vk(Vk)
    {
    }

    virtual void* CreateSharedMemory(u64) = 0;
    virtual void* CreateSharedSync() = 0;
};

} // namespace mz