// Copyright MediaZ AS. All Rights Reserved.

#include "mzVulkan/Semaphore.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/NativeAPIDirectx.h"

#undef CreateSemaphore

namespace mz::vk
{

#define HANDLE_TYPES (VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT | VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
#define HANDLE_TYPE  (VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)

Semaphore::Semaphore(Device *Vk, u64 pid, HANDLE ExtHandle) : DeviceChild(Vk)
{
    VkExportSemaphoreWin32HandleInfoKHR handleInfo = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
        .dwAccess = GENERIC_ALL,
    };

    VkExportSemaphoreCreateInfo exportInfo = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .pNext = &handleInfo,
        .handleTypes = HANDLE_TYPES,
    };

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = &exportInfo,
        .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
		.flags = VK_SEMAPHORE_TYPE_TIMELINE,
    };

    MZVK_ASSERT(Vk->CreateSemaphore(&semaphoreCreateInfo, 0, &Handle));

    if(ExtHandle)
    {
        VkImportSemaphoreWin32HandleInfoKHR importInfo = {
			.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            .semaphore = Handle,
            .handleType = HANDLE_TYPE,
            .handle = PlatformDupeHandle(pid, ExtHandle),
        };
        
        MZVK_ASSERT(Vk->ImportSemaphoreWin32HandleKHR(&importInfo));
    }

    VkSemaphoreGetWin32HandleInfoKHR getHandleInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore = Handle,
        .handleType = HANDLE_TYPE,
    };

    MZVK_ASSERT(Vk->GetSemaphoreWin32HandleKHR(&getHandleInfo, &OSHandle));
    assert(OSHandle);
    
    DWORD flags;
    WIN32_ASSERT(GetHandleInformation(OSHandle, &flags));
}


Semaphore::operator VkSemaphore() const
{
    return Handle;
}

u64 Semaphore::GetValue() const
{
    u64 val;
    MZVK_ASSERT(Vk->GetSemaphoreCounterValue(Handle, &val));
    return val;
}

Semaphore::~Semaphore()
{
    PlatformCloseHandle(OSHandle);
    Vk->DestroySemaphore(Handle, 0);
}

} // namespace mz::vk