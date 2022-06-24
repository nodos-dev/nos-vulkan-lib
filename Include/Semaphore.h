#pragma once

#include <mzVkCommon.h>

namespace mz::vk {

struct mzVulkan_API Semaphore : DeviceChild
{
    VkSemaphore Handle;
    HANDLE OSHandle;
    Semaphore(Device* Vk);
    operator VkSemaphore() const;
    ~Semaphore();
    u64 GetValue() const;
};

}  // namespace mz::vk