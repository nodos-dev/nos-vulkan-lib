/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Common.h"

namespace nos::vk
{

struct nosVulkan_API Semaphore : SharedFactory<Semaphore>, DeviceChild
{
    VkSemaphore Handle;
    HANDLE OSHandle;
    Semaphore(Device *Vk, u64 pid = 0, HANDLE OSHandle = 0);

    void Signal(uint64_t value);
    VkResult Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX);
    
    operator VkSemaphore() const;
    ~Semaphore();
    u64 GetValue() const;
};

} // namespace nos::vk