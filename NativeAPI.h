#pragma once

#include <mzCommon.h>

#include "InfoStructs.h"

namespace mz::vk
{

struct mzVulkan_API NativeAPI
{
    struct Device* Vk;

    NativeAPI(Device* Vk)
        : Vk(Vk)
    {
    }

    virtual void* CreateSharedMemory(u64) = 0;
    virtual void* CreateSharedSync() = 0;
};

} // namespace mz::vk