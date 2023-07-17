/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Common.h"

namespace mz::vk
{

struct mzVulkan_API Semaphore : SharedFactory<Semaphore>, DeviceChild
{
    VkSemaphore Handle;
    HANDLE OSHandle;
    Semaphore(Device *Vk, u64 pid = 0, HANDLE OSHandle = 0);

    void Signal(uint64_t value);
    
    operator VkSemaphore() const;
    ~Semaphore();
    u64 GetValue() const;
};

} // namespace mz::vk