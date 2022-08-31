
// External
#include <vulkan/vulkan_core.h>

// Framework
#include <mzDefines.h>

// mzVulkan
#include "mzVulkan/NativeAPIDirectx.h"
#include "mzVulkan/Common.h"
#include "mzVulkan/Allocator.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/Image.h"
#include "mzVulkan/Buffer.h"

#define GENERIC_READ    (0x80000000L)
#define GENERIC_WRITE   (0x40000000L)
#define GENERIC_EXECUTE (0x20000000L)
#define GENERIC_ALL     (0x10000000L)

static VkDeviceSize AlignUp(VkDeviceSize offset, VkDeviceSize alignment)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

static bool ChunkIsFit(VkDeviceSize offset, VkDeviceSize size, VkDeviceSize reqSize, VkDeviceSize alignment)
{
    return (size + offset - AlignUp(offset, alignment)) >= reqSize;
}

static std::string MemPropsToString(VkMemoryPropertyFlags flags)
{
    std::vector<std::string> re;

    if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        re.push_back("DEVICE_LOCAL");
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        re.push_back("HOST_VISIBLE");
    if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        re.push_back("HOST_COHERENT");
    if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
        re.push_back("HOST_CACHED");
    if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
        re.push_back("LAZILY_ALLOCATED");
    if (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
        re.push_back("PROTECTED");
    if (flags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
        re.push_back("DEVICE_COHERENT_BIT");
    if (flags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
        re.push_back("DEVICE_UNCACHED_BIT");
    if (flags & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)
        re.push_back("RDMA_CAPABLE_BI");

    if (re.empty())
        return "NONE";

    return std::accumulate(re.begin() + 1, re.end(), re.front(), [](auto a, auto b) { return a + "|" + b; });
}

namespace mz::vk
{

struct MemoryBlock : SharedFactory<MemoryBlock>
{
    std::mutex Mutex;
    Allocator* Alloc;
    VkDeviceMemory Memory;
    VkMemoryPropertyFlags Props;
    VkExternalMemoryHandleTypeFlagBits Type;
    u32 TypeIndex;

    HANDLE OSHandle;
    u8* Mapping;

    u64 Offset;
    u64 Size;
    u64 InUse;

    std::map<VkDeviceSize, VkDeviceSize> Chunks;
    std::map<VkDeviceSize, VkDeviceSize> FreeList;

    MemoryBlock(Allocator* Alloc, VkDeviceMemory mem, u64 typeIndex, VkMemoryPropertyFlags props, VkExternalMemoryHandleTypeFlagBits type, u64 offset, u64 size);
    
    ~MemoryBlock()
    {
        std::unique_lock lock(Alloc->Mutex);
        Alloc->Allocations[TypeIndex].erase(this);
        if (Alloc->Allocations[TypeIndex].empty())
        {
            Alloc->Allocations.erase(TypeIndex);
        }
        bool ok = PlatformCloseHandle(OSHandle);
        assert(ok);
        Alloc->Vk->FreeMemory(Memory, 0);
    }

    Allocation Allocate(VkDeviceSize reqSize, VkDeviceSize alignment)
    {
        std::unique_lock lock(Mutex);
        if (InUse + reqSize > Size)
        {
            return Allocation{};
        }

        auto next = FreeList.begin();

        while (next != FreeList.end() && !ChunkIsFit(next->first, next->second, reqSize, alignment))
        {
            next++;
        }

        if (next == FreeList.end())
        {
            return Allocation{};
        }

        VkDeviceSize offset = AlignUp(next->first, alignment);

        VkDeviceSize sizeFromChunkStart = reqSize + offset - next->first;

        Chunks[offset] = reqSize;

        Allocation chunk(shared_from_this(), offset, reqSize);

        if (next->second > sizeFromChunkStart)
        {
            FreeList[offset + reqSize] = next->second - sizeFromChunkStart;
        }

        if (offset == next->first)
        {
            FreeList.erase(next);
        }
        else
        {
            next->second = offset - next->first;
        }

        InUse += reqSize;
        return chunk;
    }

    void Free(Allocation alloc)
    {
        if ((alloc.Block.get() != this))
        {
            return;
        }
        std::unique_lock lock(Mutex);
        if (auto it = Chunks.find(alloc.Offset); it != Chunks.end())
        {
            Chunks.erase(it);
        }
        else
        {
            return;
        }

        InUse -= alloc.Size;

        if (auto [it, inserted] = FreeList.insert(std::make_pair(alloc.Offset, alloc.Size)); inserted)
        {
            // merge backwards
            auto prev = it;
            while (prev != FreeList.begin() && prev != FreeList.end())
            {
                --prev;
                if (prev->first + prev->second != it->first)
                {
                    break;
                }
                prev->second += it->second;
                FreeList.erase(it);
                it = prev;
            }

            // merge forwards
            auto next = std::next(it);
            while (next != FreeList.end() && it->first + it->second == next->first)
            {
                it->second += next->second;
                FreeList.erase(next++);
            }
        }
    }
};

Allocation::Allocation()
    : Block(0), Offset(0), Size(0)
{
}

Allocation::Allocation(rc<MemoryBlock> Block, u64 Offset, u64 Size)
    : Block(Block), Offset(Offset), Size(Size)
{
}

bool Allocation::IsValid() const
{
    return Block.get() != nullptr;
}

u8* Allocation::Map()
{
    if (Block->Mapping)
    {
        return Block->Mapping + Offset + Block->Offset;
    }
    return 0;
}

void Allocation::Flush()
{
    VkMappedMemoryRange range = {
        .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = Block->Memory,
        .offset = Offset + Block->Offset,
        .size   = Size,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Block->Alloc->Vk->FlushMappedMemoryRanges(1, &range));
}

void Allocation::Free()
{
    if (IsValid())
    {
        Block->Free(*this);
    }
}

void Allocation::BindResource(VkImage image)
{
    MZ_VULKAN_ASSERT_SUCCESS(Block->Alloc->Vk->BindImageMemory(image, Block->Memory, Offset + Block->Offset));
}

void Allocation::BindResource(VkBuffer buffer)
{
    MZ_VULKAN_ASSERT_SUCCESS(Block->Alloc->Vk->BindBufferMemory(buffer, Block->Memory, Offset + Block->Offset));
}

HANDLE Allocation::GetOSHandle() const
{
    return Block->OSHandle;
}

VkDeviceSize Allocation::GlobalOffset() const
{
    return Offset + Block->Offset;
}

VkDeviceSize Allocation::LocalOffset() const
{
    return Offset;
}

VkDeviceSize Allocation::LocalSize() const
{
    return Size;
}

VkDeviceSize Allocation::GlobalSize() const
{
    return Block->Size;
}

VkDeviceMemory Allocation::GetMemory() const
{
    return Block->Memory;
}

VkExternalMemoryHandleTypeFlagBits Allocation::GetType() const
{
    return Block->Type;
}

Allocator::Allocator(Device* Vk)
    : DeviceChild(Vk), Dx(new NativeAPID3D12(Vk))
{
}

Allocation Allocator::AllocateImageMemory(VkImage img, ImageCreateInfo const& info)
{
    switch(info.Type)
    {
    default: UNREACHABLE;
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT:
        return AllocateResourceMemory(img, info.Type, false, info.Imported);
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT:
        break;
    }
    
    std::lock_guard lock(Mutex);
    VkMemoryRequirements req;
    VkPhysicalDeviceMemoryProperties props;

    Vk->GetImageMemoryRequirements(img, &req);

    HANDLE memory;
    u64 offset = 0;
    if(info.Imported)
    {
        offset = info.Imported->Offset;
        memory = PlatformDupeHandle(info.Imported->PID, info.Imported->Memory);
    }
    else 
    {   
        memory = Dx->CreateSharedTexture(info.Extent, info.Format);
    }
    VkMemoryPropertyFlags requiredProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryWin32HandlePropertiesKHR handleProps = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
    };
    MZ_VULKAN_ASSERT_SUCCESS(Vk->GetMemoryWin32HandlePropertiesKHR(info.Type, memory,  &handleProps));
    auto [typeIndex, actualProps] = MemoryTypeIndex(Vk->PhysicalDevice, handleProps.memoryTypeBits, requiredProps);
    
    VkMemoryDedicatedAllocateInfo dedicated = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .image = img,
    };

    VkImportMemoryWin32HandleInfoKHR importInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
        .pNext      = &dedicated,
        .handleType = info.Type,
        .handle     = memory,
    };

    VkMemoryAllocateInfo allocateInfo = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &importInfo,
        .allocationSize  = req.size,
        .memoryTypeIndex = typeIndex,
    };

    VkDeviceMemory mem;
    MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateMemory(&allocateInfo, 0, &mem));
    auto Block = MemoryBlock::New(this, mem, typeIndex, actualProps, info.Type, offset, req.size);
    return Block->Allocate(req.size, req.alignment);
}

Allocation Allocator::AllocateResourceMemory(std::variant<VkBuffer, VkImage> resource, VkExternalMemoryHandleTypeFlagBits type, bool map, const MemoryExportInfo* imported)
{
    std::lock_guard lock(Mutex);
    VkMemoryRequirements req;
    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (auto buf = std::get_if<VkBuffer>(&resource))
    {
        if (map)
        {
            memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        }
        Vk->GetBufferMemoryRequirements(*buf, &req);
    }
    else
    {
        if(map)
        {
            memProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }
        VkImage img = std::get<VkImage>(resource);
        Vk->GetImageMemoryRequirements(img, &req);
    }

    auto [typeIndex, actualProps] = MemoryTypeIndex(Vk->PhysicalDevice, req.memoryTypeBits, memProps);

    if (imported)
    {
        HANDLE memory = PlatformDupeHandle(imported->PID, imported->Memory);

        const u64 Size = imported->Offset + req.size;

        VkImportMemoryWin32HandleInfoKHR importInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
            .handleType = type,
            .handle     = memory,
        };

        VkMemoryAllocateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = &importInfo,
            .allocationSize  = Size,
            .memoryTypeIndex = typeIndex,
        };

        VkDeviceMemory mem;
        MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateMemory(&info, 0, &mem));
        auto Block = MemoryBlock::New(this, mem, typeIndex, actualProps, type, imported->Offset, Size);
        return Block->Allocate(req.size, req.alignment);
    }

    // if(std::get_if<VkBuffer>(&resource))
    if (auto it = Allocations.find(typeIndex); it != Allocations.end())
    {
        auto& [_, blocks] = *it;

        for (auto& block : blocks)
        {
            if (auto allocation = block->Allocate(req.size, req.alignment); allocation.IsValid())
            {
                return allocation;
            }
        }
    }

    // If the value of VkExportMemoryAllocateInfo::handleTypes used to allocate memory is not 0,
    // it must include at least one of the handles set in VkExternalMemoryBufferCreateInfo::handleTypes when buffer was created

    u64 size = std::max(req.size, DefaultChunkSize);

    VkExportMemoryWin32HandleInfoKHR handleInfo = {
        .sType    = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
        .dwAccess = GENERIC_ALL,
    };

    VkExportMemoryAllocateInfo exportInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .pNext       = &handleInfo,
        .handleTypes = (VkFlags)type,
    };

    VkMemoryAllocateInfo info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &exportInfo,
        .allocationSize  = size,
        .memoryTypeIndex = typeIndex,
    };

    VkDeviceMemory mem;
    MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateMemory(&info, 0, &mem));
    rc<MemoryBlock> block = MemoryBlock::New(this, mem, typeIndex, actualProps, type, 0, size);
    Allocations[typeIndex]. insert(block.get());
    Allocation allocation = block->Allocate(req.size, req.alignment);
    return allocation;
}

MemoryBlock::MemoryBlock(Allocator* Alloc, VkDeviceMemory mem, u64 typeIndex, VkMemoryPropertyFlags props, VkExternalMemoryHandleTypeFlagBits type, u64 offset, u64 size)
    : Alloc(Alloc), Memory(mem), Props(props), Type(type), TypeIndex(typeIndex), Offset(offset), Size(size), InUse(0), Mapping(0), OSHandle(0)
{
    FreeList[0] = size;

    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        MZ_VULKAN_ASSERT_SUCCESS(Alloc->Vk->MapMemory(Memory, Offset, Size, 0, (void**)&Mapping));
    }

    VkMemoryGetWin32HandleInfoKHR getHandleInfo = {
        .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
        .memory     = mem,
        .handleType = type,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Alloc->Vk->GetMemoryWin32HandleKHR(&getHandleInfo, &OSHandle));
}

} // namespace mz::vk