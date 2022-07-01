
#pragma once

#include <NativeAPI.h>

#include <d3d12.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi.h>

#include <system_error>

extern "C" DXGI_FORMAT mzVulkan_API VkFormatToDxgiFormat(VkFormat fmt);
extern "C" VkFormat mzVulkan_API DxgiFormatToVkFormat(DXGI_FORMAT fmt);

#define MZ_D3D12_ASSERT_SUCCESS(expr)                                                                              \
    {                                                                                                              \
        HRESULT re = (expr);                                                                                       \
        while (FAILED(re))                                                                                         \
        {                                                                                                          \
            std::string __err = std::system_category().message(re);                                                \
            printf("[%lx]Error: %s\n%s:%d\n", re, __err.c_str(), __FILE__, __LINE__);                              \
            abort();                                                                                               \
        }                                                                                                          \
    }

#define WIN32_ASSERT(expr)                                                                   \
    if (!(expr))                                                                             \
    {                                                                                        \
        printf("%s\t%s:%d\n", ::mz::vk::GetLastErrorAsString().c_str(), __FILE__, __LINE__); \
        abort();                                                                             \
    }

namespace mz::vk
{

struct mzVulkan_API NativeAPID3D12 : NativeAPI
{
    ID3D12Device* Dx12;
    NativeAPID3D12(Device* Vk);
    
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/shared-heaps#sharing-heaps-across-processes
    virtual void* CreateSharedMemory(u64 size) override;
    virtual void* CreateSharedSync() override;
    virtual void* CreateSharedTexture(VkExtent2D extent, VkFormat format) override;
};

struct mzVulkan_API NativeAPID3D11 : NativeAPI
{
    ID3D11Device* Dx11;
    ID3D11Device5* Dx11_5;
    ID3D11DeviceContext* Ctx;
    NativeAPID3D11(Device* Vk);
    virtual void* CreateSharedMemory(u64 size) override;
    virtual void* CreateSharedSync() override;
    virtual void* CreateSharedTexture(VkExtent2D extent, VkFormat format) override;
};

}; // namespace mz::vk