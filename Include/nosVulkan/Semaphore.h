/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Common.h"
#include <cstdint>

namespace nos::vk
{

struct nosVulkan_API Semaphore : SharedFactory<Semaphore>, DeviceChild
{
	VkSemaphore Handle = VK_NULL_HANDLE;
	VkSemaphoreType Type;
	NOS_HANDLE OSHandle{};
	u64 PID{};
    Semaphore(Device *Vk, VkSemaphoreType type, u64 pid = 0, NOS_HANDLE OSHandle = 0);

    void Signal(uint64_t value);
    VkResult Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX);
    
    operator VkSemaphore() const;
    ~Semaphore();
    u64 GetValue() const;
};

} // namespace nos::vk