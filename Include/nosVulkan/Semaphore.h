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
	std::optional<NOS_HANDLE> OsHandle; // If an imported semaphore is exported, they are the same handle.
	std::optional<u64> ImportedPid;
    // TODO: Image/Buffer resource like creation that handles failures
    Semaphore(Device* Vk, VkSemaphoreType type, bool shouldExport, uint64_t importedSourcePid = 0, NOS_HANDLE importOsHandle = {});

    void Signal(uint64_t value);
    VkResult Wait(uint64_t value, uint64_t timeoutNs = UINT64_MAX);
    
    operator VkSemaphore() const;
    ~Semaphore();
    u64 GetValue() const;
};

} // namespace nos::vk