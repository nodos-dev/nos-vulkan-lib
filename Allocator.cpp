#include "Allocator.h"
#include "Device.h"

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

std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(VkPhysicalDevice physicalDevice, u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps)
{

    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);

    u32 typeIndex = 0;

    if (0 == requestedProps)
    {
        typeIndex = log2(memoryTypeBits - (memoryTypeBits & (memoryTypeBits - 1)));
    }
    else
    {
        std::vector<std::pair<u32, u32>> memoryTypes;

        for (int i = 0; i < props.memoryTypeCount; i++)
        {
            if (memoryTypeBits & (1 << i))
            {
                memoryTypes.push_back(std::make_pair(i, std::popcount(props.memoryTypes[i].propertyFlags & requestedProps)));
            }
        }

        std::sort(memoryTypes.begin(), memoryTypes.end(), [](const std::pair<u32, u32>& a, const std::pair<u32, u32>& b) { return a.second > b.second; });

        typeIndex = memoryTypes.front().first;
    }

    return std::make_pair(typeIndex, props.memoryTypes[typeIndex].propertyFlags);
}

Allocator::Allocator(Device* Vk)
    : Vk(Vk)
{
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
} // namespace mz::vk