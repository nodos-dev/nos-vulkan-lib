// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "nosVulkan/Semaphore.h"
#include "nosVulkan/Device.h"

#include "nosVulkan/Platform.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

#undef CreateSemaphore

namespace nos::vk
{

#if defined(_WIN32)
#define HANDLE_TYPE  (VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)
#elif defined(__linux__)
#define HANDLE_TYPE (VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
#endif
Semaphore::Semaphore(Device* Vk, VkSemaphoreType type, u64 pid, NOS_HANDLE ExtHandle) 
    : DeviceChild(Vk), Type(type)
{
#if defined(_WIN32)
    VkExportSemaphoreWin32HandleInfoKHR handleInfo = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
        .dwAccess = GENERIC_ALL,
    };

    VkExportSemaphoreCreateInfo exportInfo = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .pNext = &handleInfo,
        .handleTypes = HANDLE_TYPE,
    };
#elif defined(__linux__)

    VkExportSemaphoreCreateInfo exportInfo = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .handleTypes = HANDLE_TYPE,
    };
#endif

	VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = &exportInfo,
		.semaphoreType = type,
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
        #if defined(_WIN32)
        VkImportSemaphoreWin32HandleInfoKHR importInfo = {
			.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            .semaphore = Handle,
            .handleType = HANDLE_TYPE,
            .handle = PlatformDupeHandle(pid, ExtHandle),
        };
        NOSVK_ASSERT(Vk->ImportSemaphoreWin32HandleKHR(&importInfo));
        #elif defined(__linux__)
        VkImportSemaphoreFdInfoKHR importInfo = {
			.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
            .semaphore = Handle,
            .handleType = HANDLE_TYPE,
            .fd = PlatformDupeHandle(pid, ExtHandle),
        };
        NOSVK_ASSERT(Vk->ImportSemaphoreFdKHR(&importInfo));
        #endif
        
    }
#if defined(_WIN32)
    VkSemaphoreGetWin32HandleInfoKHR getHandleInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore = Handle,
        .handleType = HANDLE_TYPE,
    };

	NOSVK_ASSERT(Vk->GetSemaphoreWin32HandleKHR(&getHandleInfo, &OSHandle));
#elif defined(__linux__)
    VkSemaphoreGetFdInfoKHR getHandleInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
        .semaphore = Handle,
        .handleType = HANDLE_TYPE,
    };

	NOSVK_ASSERT(Vk->GetSemaphoreFdKHR(&getHandleInfo, &OSHandle));

#endif
    assert(OSHandle);

#if _WIN32
	DWORD flags;
	WIN32_ASSERT(GetHandleInformation(OSHandle, &flags));
#else
	 //TODO LINUX_SUPPORT
#endif

	if (!pid)
        pid = PlatformGetCurrentProcessId();
	PID = pid;
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
	if (OSHandle)
        PlatformCloseHandle(OSHandle);
    Vk->DestroySemaphore(Handle, 0);
}

} // namespace nos::vk