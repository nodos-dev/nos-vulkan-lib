// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "nosVulkan/Semaphore.h"
#include "nosVulkan/Device.h"

#include "nosVulkan/Platform.h"

#if defined(_WIN32)
#include <Windows.h>
#else if defined(__linux)
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#undef CreateSemaphore

namespace nos::vk
{

void CheckOsStatsForSemaphoreCreationFailure();
#if defined(_WIN32)
#define HANDLE_TYPE  (VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)
void CheckOsStatsForSemaphoreCreationFailure(){}
#elif defined(__linux__) || defined(__APPLE__)
#define HANDLE_TYPE (VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
void CheckOsStatsForSemaphoreCreationFailure(){
    struct rlimit limit;
    getrlimit(RLIMIT_NOFILE, &limit);
    int count = 0;
    for (int i = 0; i < limit.rlim_cur; ++i)
    {
        if (fcntl(i, F_GETFD) != -1)
            count++;
    }
    if(count == limit.rlim_cur)
    {
        GLog.E("Semaphore creation failed, file descriptor limit reached: %d", count);
    }
}
#endif
Semaphore::Semaphore(Device* Vk, VkSemaphoreType type, bool shouldExport, std::optional<uint64_t> importedSourcePid, std::optional<NOS_HANDLE> importOsHandle)
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
#elif defined(__linux__) || defined(__APPLE__)
    VkExportSemaphoreCreateInfo exportInfo = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .handleTypes = HANDLE_TYPE,
    };
#endif

	VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = shouldExport ? &exportInfo : nullptr,
		.semaphoreType = type,
        .initialValue = 0,
	};

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
		.flags = 0,
    };

    auto res = Vk->CreateSemaphore(&semaphoreCreateInfo, 0, &Handle);
    if (res == VK_ERROR_INITIALIZATION_FAILED || res == VK_ERROR_DEVICE_LOST) {
        CheckOsStatsForSemaphoreCreationFailure();
        Handle = NOS_VULKAN_INVALID_HANDLE(VkSemaphore);
        return;
    }
    NOSVK_ASSERT(res);
    if(importOsHandle)
    {
        if(!importedSourcePid)
			importedSourcePid = vk::PlatformGetCurrentProcessId();
		auto importedHandle = GHandleImporter.DuplicateHandle(*importedSourcePid, *importOsHandle);
		NOS_ASSERT(importedHandle);
		if (importedHandle)
		{
			OsHandle = *importedHandle;
            ImportedPid = importedSourcePid;
#if defined(_WIN32)
			VkImportSemaphoreWin32HandleInfoKHR importInfo = {
				.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
				.semaphore = Handle,
				.handleType = HANDLE_TYPE,
				.handle = *OsHandle,
			};
			NOSVK_ASSERT(Vk->ImportSemaphoreWin32HandleKHR(&importInfo));
#elif defined(__linux__) || defined(__APPLE__)
			VkImportSemaphoreFdInfoKHR importInfo = {
				.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
				.semaphore = Handle,
				.handleType = HANDLE_TYPE,
				.fd = int(*OsHandle),
			};
			NOSVK_ASSERT(Vk->ImportSemaphoreFdKHR(&importInfo));
#endif
		}
        
    }
    if (!importOsHandle && shouldExport){
        #if defined(_WIN32)
            VkSemaphoreGetWin32HandleInfoKHR getHandleInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
                .semaphore = Handle,
                .handleType = HANDLE_TYPE,
            };
            NOS_HANDLE exportedOsHandle = {};
            NOSVK_ASSERT(Vk->GetSemaphoreWin32HandleKHR(&getHandleInfo, &exportedOsHandle));
            assert(exportedOsHandle);
			OsHandle = exportedOsHandle;
        #elif defined(__linux__)
            VkSemaphoreGetFdInfoKHR getHandleInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
                .semaphore = Handle,
                .handleType = HANDLE_TYPE,
            };
        
            int fd = 0;
            NOSVK_ASSERT(Vk->GetSemaphoreFdKHR(&getHandleInfo, &fd));
            OsHandle = NOS_HANDLE(fd);
        #endif
    }
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
	if (Handle == NOS_VULKAN_INVALID_HANDLE(VkSemaphore))
		return 0;

    u64 val;
    NOSVK_ASSERT(Vk->GetSemaphoreCounterValue(Handle, &val));
    return val;
}

Semaphore::~Semaphore()
{
	if (OsHandle)
        GHandleImporter.CloseHandle(*OsHandle);
    if (Handle != NOS_VULKAN_INVALID_HANDLE(VkSemaphore))
        Vk->DestroySemaphore(Handle, 0);
}

} // namespace nos::vk