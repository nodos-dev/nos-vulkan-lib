
#pragma once

#include <NativeAPI.h>

#include <d3d12.h>
#include <dxgi.h>

#include <system_error>

extern "C" mzVulkan_API const VkFormat DXGI_FORMAT_TO_VK_FORMAT[];
extern "C" mzVulkan_API const DXGI_FORMAT VK_FORMAT_TO_DXGI_FORMAT[];

#define MZ_D3D12_ASSERT_SUCCESS(expr)                                                                              \
    {                                                                                                              \
        HRESULT re = (expr);                                                                                       \
        while (FAILED(re))                                                                                         \
        {                                                                                                          \
            printf("[%lx]Error: %s\n%s:%d\n", re, std::system_category().message(re).c_str(), __FILE__, __LINE__); \
            abort();                                                                                               \
        }                                                                                                          \
    }

#define WIN32_ASSERT(expr)                                                                   \
    if (!(expr))                                                                             \
    {                                                                                        \
        printf("%s(%s:%d)\n", ::mz::vk::GetLastErrorAsString().c_str(), __FILE__, __LINE__); \
        abort();                                                                             \
    }

namespace mz::vk
{

struct mzVulkan_API NativeAPID3D12 : NativeAPI
{
    ID3D12Device* dx12;

    NativeAPID3D12(Device* Vk);

    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/shared-heaps#sharing-heaps-across-processes
    virtual void* CreateSharedMemory(u64 size) override;
    virtual void* CreateSharedSync() override;
};

}; // namespace mz::vk