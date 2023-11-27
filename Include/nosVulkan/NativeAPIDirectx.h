/*
 * Copyright MediaZ AS. All Rights Reserved.
 */


#pragma once

// std
#include <cstdio>
#include <system_error>

// External
#include <d3d12.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi.h>

// nosVulkan
#include "NativeAPI.h"

extern "C" DXGI_FORMAT nosVulkan_API VkFormatToDxgiFormat(VkFormat fmt);
extern "C" VkFormat nosVulkan_API DxgiFormatToVkFormat(DXGI_FORMAT fmt);

#define NOS_D3D12_ASSERT_SUCCESS(expr)                                                               \
    {                                                                                               \
        HRESULT re = (expr);                                                                        \
        while (FAILED(re))                                                                          \
        {                                                                                           \
            std::string __err = std::system_category().message(re);                                 \
            char errbuf[1024];                                                                      \
            std::snprintf(errbuf, 1024, "[%lx] %s (%s:%d)", re, __err.c_str(), __FILE__, __LINE__); \
            assert(false);                                                                           \
        }                                                                                           \
    }

#define WIN32_ASSERT(expr)                                                                                        \
    if (!(expr))                                                                                                  \
    {                                                                                                             \
        char errbuf[1024];                                                                                        \
        std::snprintf(errbuf, 1024, "%s\t(%s:%d)", ::nos::vk::GetLastErrorAsString().c_str(), __FILE__, __LINE__); \
		assert(false);                                                                                            \
    }

namespace nos::vk
{

struct nosVulkan_API NativeAPID3D12 : NativeAPI
{
    ID3D12Device* Dx12;
    NativeAPID3D12(Device* Vk);
    
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/shared-heaps#sharing-heaps-across-processes
    virtual void* CreateSharedMemory(u64 size) override;
    virtual void* CreateSharedSync() override;
    virtual void* CreateSharedTexture(VkExtent2D extent, VkFormat format) override;
};

struct nosVulkan_API NativeAPID3D11 : NativeAPI
{
    ID3D11Device* Dx11;
    ID3D11Device5* Dx11_5;
    ID3D11DeviceContext* Ctx;
    NativeAPID3D11(Device* Vk);
    virtual void* CreateSharedMemory(u64 size) override;
    virtual void* CreateSharedSync() override;
    virtual void* CreateSharedTexture(VkExtent2D extent, VkFormat format) override;
};

}; // namespace nos::vk