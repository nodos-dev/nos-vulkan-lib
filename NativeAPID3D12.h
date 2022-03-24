
#pragma once

#include "NativeAPI.h"

#include "Image.h"

#include <d3d12.h>
#include <dxgi.h>

#include <system_error>

extern "C" MZVULKAN_API const VkFormat DXGI_FORMAT_TO_VK_FORMAT[];
extern "C" MZVULKAN_API const DXGI_FORMAT VK_FORMAT_TO_DXGI_FORMAT[];

#define MZ_D3D12_ASSERT_SUCCESS(expr)                                                                              \
    {                                                                                                              \
        HRESULT re = (expr);                                                                                       \
        while (FAILED(re))                                                                                         \
        {                                                                                                          \
            printf("[%lx]Error: %s\n%s:%d\n", re, std::system_category().message(re).c_str(), __FILE__, __LINE__); \
            abort();                                                                                               \
        }                                                                                                          \
    }

#define WIN32_ASSERT(expr)                                                         \
    if (!(expr))                                                                   \
    {                                                                              \
        printf("%s(%s:%d)\n", GetLastErrorAsString().c_str(), __FILE__, __LINE__); \
        abort();                                                                   \
    }

inline std::string GetLastErrorAsString()
{
    // Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
    {
        return std::string(); // No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL,
                                 errorMessageID,
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 (LPSTR)&messageBuffer,
                                 0,
                                 NULL);

    // Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

namespace mz::vk
{

struct MZVULKAN_API NativeAPID3D12 : NativeAPI
{
    ID3D12Device* dx12;

    NativeAPID3D12(Device* Vk)
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

}; // namespace mz::vk