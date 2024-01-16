/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Common.h"

namespace nos::vk
{

struct nosVulkan_API Semaphore : SharedFactory<Semaphore>, DeviceChild
{
    VkSemaphore Handle;
	bool Timeline;
	HANDLE OSHandle{};
    Semaphore(Device *Vk, bool timeline, u64 pid = 0, HANDLE OSHandle = 0);

    void Signal(uint64_t value);
    VkResult Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX);
    
    operator VkSemaphore() const;
    ~Semaphore();
    u64 GetValue() const;
};

} // namespace nos::vk