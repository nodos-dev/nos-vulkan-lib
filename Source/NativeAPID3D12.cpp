#include "vulkan/vulkan_core.h"
#include <NativeAPID3D12.h>
#include <combaseapi.h>

namespace mz::vk
{
  
NativeAPID3D11::NativeAPID3D11(Device* Vk)
    : NativeAPI(Vk)
{
    IDXGIFactory* pDXGIFactory = 0;
    IDXGIAdapter* pDXGIAdapter = 0;
    IDXGIAdapter* pNextDXGIAdapter = 0;

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

    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    // Create D3D12 device
    MZ_D3D12_ASSERT_SUCCESS(
      D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, featureLevels, 4, D3D11_SDK_VERSION, &Dx11, &featureLevel, &Ctx)
      );

    pDXGIFactory->Release();
    pDXGIAdapter->Release();
}

NativeAPID3D12::NativeAPID3D12(Device* Vk)
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
    MZ_D3D12_ASSERT_SUCCESS(D3D12CreateDevice(pDXGIAdapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), reinterpret_cast<void**>(&Dx12)));

    pDXGIFactory->Release();
    pDXGIAdapter->Release();
}

void* NativeAPID3D12::CreateSharedMemory(u64 size)
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
    HANDLE handle;

    HRESULT hr = Dx12->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), 0);


    MZ_D3D12_ASSERT_SUCCESS(Dx12->CreateHeap(&heapDesc, __uuidof(ID3D12Heap), (void**)&heap));

    MZ_D3D12_ASSERT_SUCCESS(Dx12->CreateSharedHandle(heap, 0, GENERIC_ALL, 0, &handle));

    heap->Release();

    return handle;
}

void* NativeAPID3D12::CreateSharedTexture(VkExtent2D extent, VkFormat format)
{
    // Shared heaps are not supported on CPU-accessible heaps
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment        = 0;
    desc.Width            = extent.width;
    desc.Height           = extent.height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = VK_FORMAT_TO_DXGI_FORMAT[format];
    desc.SampleDesc.Count = 1;
    desc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    ID3D12Resource* res;
    HANDLE handle;
    
    D3D12_HEAP_PROPERTIES props = {.Type = D3D12_HEAP_TYPE_DEFAULT};
    HRESULT hr = Dx12->CreateCommittedResource(
        &props, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COMMON, 0,
        IID_PPV_ARGS(&res));

    MZ_D3D12_ASSERT_SUCCESS(Dx12->CreateSharedHandle(res, 0, GENERIC_ALL, 0, &handle));
    res->Release();
    
    return handle;
}

void* NativeAPID3D12::CreateSharedSync()
{
    HANDLE handle;
    ID3D12Fence* fence;

    MZ_D3D12_ASSERT_SUCCESS(Dx12->CreateFence(0, D3D12_FENCE_FLAG_SHARED, __uuidof(ID3D12Fence), (void**)(&fence)));

    MZ_D3D12_ASSERT_SUCCESS(Dx12->CreateSharedHandle(fence, 0, GENERIC_ALL, 0, &handle));

    fence->Release();

    return handle;
}

} // namespace mz::vk