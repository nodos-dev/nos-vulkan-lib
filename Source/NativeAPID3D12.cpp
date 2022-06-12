
#include <NativeAPIDirectx.h>
#include <combaseapi.h>
#include <d3d11.h>

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
    std::vector<D3D_FEATURE_LEVEL> featureLevels =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    MZ_D3D12_ASSERT_SUCCESS(D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, featureLevels.data(), featureLevels.size(), D3D11_SDK_VERSION, &Dx11, &featureLevel, &Ctx));
    MZ_D3D12_ASSERT_SUCCESS(Dx11->QueryInterface(IID_PPV_ARGS(&Dx11_5)));
    pDXGIFactory->Release();
    pDXGIAdapter->Release();
}


void* NativeAPID3D11::CreateSharedMemory(u64 size)  
{
  // Figure this out
  return 0;
}

void* NativeAPID3D11::CreateSharedSync() 
{
  HANDLE handle = 0;
  ID3D11Fence* pFence = 0;

  MZ_D3D12_ASSERT_SUCCESS(Dx11_5->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&pFence)));
  MZ_D3D12_ASSERT_SUCCESS(pFence->CreateSharedHandle(0, GENERIC_ALL, 0, &handle));

  pFence->Release();

  return handle;
}

void* NativeAPID3D11::CreateSharedTexture(VkExtent2D extent, VkFormat format) 
{ 

  HANDLE handle = 0;
  ID3D11Texture2D* pTexture = 0;
  IDXGIResource * pDXGIResource = 0;
  D3D11_TEXTURE2D_DESC desc = {
    .Width = extent.width,
    .Height = extent.height,
    .MipLevels = 1,
    .ArraySize = 1,
    .Format = VK_FORMAT_TO_DXGI_FORMAT[format],
    .SampleDesc = { .Count = 1, .Quality = 0 },
    .Usage = D3D11_USAGE_DEFAULT,
    .BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS,
    .CPUAccessFlags = 0,
    .MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
  };

  MZ_D3D12_ASSERT_SUCCESS(Dx11_5->CreateTexture2D(&desc, 0, &pTexture));
  MZ_D3D12_ASSERT_SUCCESS(pTexture->QueryInterface(IID_PPV_ARGS(&pDXGIResource)));

  pTexture->Release();
  pDXGIResource->Release();

  return handle;
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