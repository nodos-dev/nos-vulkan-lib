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
	struct ImportInfo
	{
		NOS_HANDLE OsHandle{};
		u64 Pid = 0;
	};
	VkSemaphore Handle = VK_NULL_HANDLE;
	VkSemaphoreType Type;
    NOS_HANDLE LocalSemaphoreOsHandle{}; // OS handle of the vulkan semaphore created
	ImportInfo Imported{};
	Semaphore(Device* Vk, VkSemaphoreType type, bool shouldExport, ImportInfo importInfo = {});

    void Signal(uint64_t value);
    VkResult Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX);
    
    operator VkSemaphore() const;
    ~Semaphore();
    u64 GetValue() const;
};

} // namespace nos::vk