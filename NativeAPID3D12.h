
#pragma once

#include "NativeAPI.h"

#include "Image.h"

#include <d3d12.h>
#include <dxgi.h>

#include <system_error>

extern "C" const VkFormat    DXGI_FORMAT_TO_VK_FORMAT[];
extern "C" const DXGI_FORMAT VK_FORMAT_TO_DXGI_FORMAT[];

#define MZ_D3D12_ASSERT_SUCCESS(expr)                                                                              \
    {                                                                                                              \
        HRESULT re = (expr);                                                                                       \
        while (FAILED(re))                                                                                         \
        {                                                                                                          \
            printf("[%lx]Error: %s\n%s:%d\n", re, std::system_category().message(re).c_str(), __FILE__, __LINE__); \
            abort();                                                                                               \
        }                                                                                                          \
    }

namespace mz
{

struct NativeAPID3D12 : NativeAPI
{
    ID3D12Device* dx12;

    NativeAPID3D12(VulkanDevice* Vk)
        : NativeAPI(Vk)
    {
        IDXGIFactory* pDXGIFactory;
        IDXGIAdapter* pDXGIAdapter;
        IDXGIAdapter* pNextDXGIAdapter;

        u64 luid = Vk->GetLuid();

        // Find adapter matching LUID
        MZ_D3D12_ASSERT_SUCCESS(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pDXGIFactory)));

        for (uint32_t i = 0; SUCCEEDED(pDXGIFactory->EnumAdapters(i, &pNextDXGIAdapter)); ++i)
        {
            DXGI_ADAPTER_DESC desc;
            MZ_D3D12_ASSERT_SUCCESS(pNextDXGIAdapter->GetDesc(&desc));
            if ((desc.AdapterLuid.HighPart == luid >> 32) && (desc.AdapterLuid.LowPart == (luid & 0xFFFFFFFF)))
            {
                pDXGIAdapter = pNextDXGIAdapter;
                break;
            }
        }

        // Create D3D12 device
        MZ_D3D12_ASSERT_SUCCESS(D3D12CreateDevice(pDXGIAdapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&dx12)));

        pDXGIFactory->Release();
        pDXGIAdapter->Release();
    }

    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/shared-heaps#sharing-heaps-across-processes
    virtual void* CreateSharedMemory(u64 size) override
    {
        // Shared heaps are not supported on CPU-accessible heaps
        D3D12_HEAP_DESC heapDesc = {
            .SizeInBytes = size,
            .Properties  = {
                .Type                 = D3D12_HEAP_TYPE_DEFAULT,
                .CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            },
            .Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
            .Flags     = D3D12_HEAP_FLAG_SHARED,
        };

        ID3D12Heap* heap;
        HANDLE      handle;

        assert(S_FALSE == dx12->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), 0));

        MZ_D3D12_ASSERT_SUCCESS(dx12->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), (void**)&heap));

        MZ_D3D12_ASSERT_SUCCESS(dx12->CreateSharedHandle(heap, 0, GENERIC_ALL, 0, &handle));

        heap->Release();

        return handle;
    }

    virtual void* CreateSharedSync() override
    {
        HANDLE       handle;
        ID3D12Fence* fence;

        MZ_D3D12_ASSERT_SUCCESS(dx12->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence), (void**)(&fence)));

        MZ_D3D12_ASSERT_SUCCESS(dx12->CreateSharedHandle(fence, 0, GENERIC_ALL, 0, &handle));

        fence->Release();

        return handle;
    }
};

}; // namespace mz