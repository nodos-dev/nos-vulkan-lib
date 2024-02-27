// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "nosVulkan/Semaphore.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/NativeAPIDirectx.h"

#undef CreateSemaphore

namespace nos::vk
{

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
        .handleTypes = HANDLE_TYPE,
    };

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = &exportInfo,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
		.flags = 0,
    };

    NOSVK_ASSERT(Vk->CreateSemaphore(&semaphoreCreateInfo, 0, &Handle));

    if(ExtHandle)
    {
        VkImportSemaphoreWin32HandleInfoKHR importInfo = {
			.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            .semaphore = Handle,
            .handleType = HANDLE_TYPE,
            .handle = PlatformDupeHandle(pid, ExtHandle),
        };
        
        NOSVK_ASSERT(Vk->ImportSemaphoreWin32HandleKHR(&importInfo));
    }

    VkSemaphoreGetWin32HandleInfoKHR getHandleInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore = Handle,
        .handleType = HANDLE_TYPE,
    };

    NOSVK_ASSERT(Vk->GetSemaphoreWin32HandleKHR(&getHandleInfo, &OSHandle));
    assert(OSHandle);
    
    DWORD flags;
    WIN32_ASSERT(GetHandleInformation(OSHandle, &flags));
}

void Semaphore::Signal(uint64_t value)
{
    VkSemaphoreSignalInfo signalInfo{};
	signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	signalInfo.semaphore = Handle;
	signalInfo.value = value;

	Vk->SignalSemaphore(&signalInfo);
}

VkResult Semaphore::Wait(uint64_t value, uint64_t timeoutNs)
{
    VkSemaphoreWaitInfo waitInfo{};
	waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores = &Handle;
    waitInfo.pValues = &value;

    return Vk->WaitSemaphores(&waitInfo, timeoutNs);
}


Semaphore::operator VkSemaphore() const
{
    return Handle;
}

u64 Semaphore::GetValue() const
{
    u64 val;
    NOSVK_ASSERT(Vk->GetSemaphoreCounterValue(Handle, &val));
    return val;
}

Semaphore::~Semaphore()
{
    PlatformCloseHandle(OSHandle);
    Vk->DestroySemaphore(Handle, 0);
}

} // namespace nos::vk