#include <Allocator.h>

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

Allocation MemoryBlock::Allocate(VkDeviceSize reqSize, VkDeviceSize alignment)
{
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

void MemoryBlock::Free(Allocation c)
{

    if ((c.Block.get() != this) || Imported)
    {
        return;
    }

    if (auto it = Chunks.find(c.Offset); it != Chunks.end())
    {
        Chunks.erase(it);
    }
    else
    {
        return;
    }

    InUse -= c.Size;

    if (auto [it, inserted] = FreeList.insert(std::make_pair(c.Offset, c.Size)); inserted)
    {
        // merge backwards
        auto prev = it;
        while (prev-- != FreeList.begin() && prev->first + prev->second == it->first)
        {
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

Allocator::Allocator(Device* Vk)
    : Vk(Vk)
{
}

Device* Allocator::GetDevice() const
{
    return Vk;
}

Allocation Allocator::AllocateResourceMemory(std::variant<VkBuffer, VkImage> resource, bool map, const ImageExportInfo* imported)
{
    VkMemoryRequirements req;
    VkPhysicalDeviceMemoryProperties props;

    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (std::holds_alternative<VkBuffer>(resource))
    {
        if (map)
        {
            memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        }
        Vk->GetBufferMemoryRequirements(std::get<VkBuffer>(resource), &req);
    }
    else
    {
        Vk->GetImageMemoryRequirements(std::get<VkImage>(resource), &req);
    }

    auto [typeIndex, actualProps] = MemoryTypeIndex(Vk->PhysicalDevice, req.memoryTypeBits, memProps);

    Allocation allocation = {};

    if (imported)
    {
        VkImportMemoryWin32HandleInfoKHR importInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            .handle     = imported->memory,
        };

        VkMemoryAllocateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = &importInfo,
            .allocationSize  = imported->size,
            .memoryTypeIndex = typeIndex,
        };

        VkDeviceMemory mem;
        MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateMemory(&info, 0, &mem));
        auto Block = MemoryBlock::New(Vk, mem, actualProps, imported->offset, info.allocationSize, imported->memory);

        return Block->Allocate(req.size, req.alignment);
    }

    if (auto it = Allocations.find(typeIndex); it != Allocations.end())
    {
        auto& [_, blocks] = *it;

        for (auto& block : blocks)
        {
            if ((allocation = block->Allocate(req.size, req.alignment)).IsValid())
            {
                break;
            }
        }
    }

    // If the value of VkExportMemoryAllocateInfo::handleTypes used to allocate memory is not 0,
    // it must include at least one of the handles set in VkExternalMemoryBufferCreateInfo::handleTypes when buffer was created

    if (!allocation.IsValid())
    {
        VkDeviceMemory mem;

        u64 size = std::max(req.size, DefaultChunkSize);

        VkExportMemoryWin32HandleInfoKHR handleInfo = {
            .sType    = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
            .dwAccess = GENERIC_ALL,
        };

        VkExportMemoryAllocateInfo exportInfo = {
            .sType       = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
            .pNext       = &handleInfo,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
        };

        VkMemoryAllocateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = &exportInfo,
            .allocationSize  = size,
            .memoryTypeIndex = typeIndex,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateMemory(&info, 0, &mem));

        auto block = MemoryBlock::New(Vk, mem, actualProps, 0, size, nullptr);

        allocation = block->Allocate(req.size, req.alignment);
        Allocations[typeIndex].emplace_back(block);
    }

    return allocation;
}

MemoryBlock::MemoryBlock(Device* Vk, VkDeviceMemory mem, VkMemoryPropertyFlags props, u64 offset, u64 size, HANDLE externalHandle)
    : Vk(Vk), Memory(mem), Props(props), Offset(offset), Size(size), InUse(0), Mapping(0), Imported(externalHandle != 0)
{
    FreeList[0] = size;

    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        MZ_VULKAN_ASSERT_SUCCESS(Vk->MapMemory(Memory, Offset, Size, 0, (void**)&Mapping));
    }

    VkMemoryGetWin32HandleInfoKHR handleInfo = {
        .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
        .memory     = Memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->GetMemoryWin32HandleKHR(&handleInfo, &OSHandle));
}

MemoryBlock::~MemoryBlock()
{

    Vk->FreeMemory(Memory, 0);

    // Imported blocks do not need to decrease the refcount
    if (!Imported)
    {
        assert(PlatformClosehandle(OSHandle));
    }
}

Allocation::Allocation()
    : Block(0), Offset(0), Size(0)
{
}

Allocation::Allocation(std::shared_ptr<MemoryBlock> Block, u64 Offset, u64 Size)
    : Block(Block), Offset(Offset), Size(Size)
{
}

bool Allocation::IsValid() const
{
    return Block.get() != nullptr;
}

bool Allocation::IsImported() const
{
    return Block->Imported;
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

    MZ_VULKAN_ASSERT_SUCCESS(Block->Vk->FlushMappedMemoryRanges(1, &range));
}

HANDLE Allocation::GetOSHandle() const
{
    assert(Block->OSHandle);

    return Block->OSHandle;
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
    MZ_VULKAN_ASSERT_SUCCESS(Block->Vk->BindImageMemory(image, Block->Memory, Offset + Block->Offset));
}

void Allocation::BindResource(VkBuffer buffer)
{
    MZ_VULKAN_ASSERT_SUCCESS(Block->Vk->BindBufferMemory(buffer, Block->Memory, Offset + Block->Offset));
}

} // namespace mz::vk