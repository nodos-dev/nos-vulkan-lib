#include "mzVulkan/Semaphore.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/NativeAPIDirectx.h"

#undef CreateSemaphore

namespace mz::vk
{

Semaphore::Semaphore(Device* Vk) : DeviceChild(Vk)
{
    VkExportSemaphoreWin32HandleInfoKHR handleInfo = {
        .sType    = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
        .dwAccess = GENERIC_ALL,
    };

    VkExportSemaphoreCreateInfo exportInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .pNext       = &handleInfo,
        .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
    };

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext         = &exportInfo,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSemaphore(&semaphoreCreateInfo, 0, &Handle));

    VkSemaphoreGetWin32HandleInfoKHR getHandleInfo = {
        .sType      = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore  = Handle,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->GetSemaphoreWin32HandleKHR(&getHandleInfo, &OSHandle));
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
    MZ_VULKAN_ASSERT_SUCCESS(Vk->GetSemaphoreCounterValue(Handle, &val));
    return val;
}

Semaphore::~Semaphore()
{
    PlatformCloseHandle(OSHandle);
    Vk->DestroySemaphore(Handle, 0);
}

}