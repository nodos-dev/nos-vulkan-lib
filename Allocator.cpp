#include "Allocator.h"

Allocation MemoryBlock::Allocate(u64 size)
{
    if (InUse + size > Size)
    {
        return Allocation{};
    }

    if (IsImported())
    {
        return Allocation{shared_from_this(), 0, Size};
    }

    auto next = FreeList.begin();

    while (next != FreeList.end() && next->second < size)
    {
        next++;
    }

    if (next == FreeList.end())
    {
        return Allocation{};
    }

    Chunks[next->first] = size;

    Allocation chunk(shared_from_this(), next->first, size);

    if (next->second > size)
    {
        FreeList[next->first + size] = next->second - size;
    }

    FreeList.erase(next);

    InUse += size;
    return chunk;
}

void MemoryBlock::Free(Allocation c)
{
    if (c.Block.get() != this || IsImported())
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

std::pair<u32, VkMemoryPropertyFlags> VulkanAllocator::MemoryTypeIndex(u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps)
{

    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(Vk->PhysicalDevice, &props);

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

Allocation VulkanAllocator::ImportResourceMemory(VkBuffer buf, HANDLE handle)
{
    return ImportResourceMemoryImpl<VkBuffer>(buf, handle);
}

Allocation VulkanAllocator::ImportResourceMemory(VkImage img, HANDLE handle)
{
    return ImportResourceMemoryImpl<VkImage>(img, handle);
}

Allocation VulkanAllocator::AllocateResourceMemory(VkBuffer buf, u8** mapping)
{
    return AllocateResourceMemoryImpl<VkBuffer>(buf, mapping);
}

Allocation VulkanAllocator::AllocateResourceMemory(VkImage img, u8** mapping)
{
    return AllocateResourceMemoryImpl<VkImage>(img, mapping);
}

template <class Resource>
requires(std::is_same_v<Resource, VkBuffer> || std::is_same_v<Resource, VkImage>)
    Allocation VulkanAllocator::ImportResourceMemoryImpl(Resource resource, HANDLE handle)
{
    VkMemoryRequirements             req;
    VkPhysicalDeviceMemoryProperties props;

    if constexpr (std::is_same_v<Resource, VkBuffer>)
    {
        Vk->GetBufferMemoryRequirements(resource, &req);
    }
    else
    {
        Vk->GetImageMemoryRequirements(resource, &req);
    }

    auto [typeIndex, actualProps] = MemoryTypeIndex(req.memoryTypeBits, 0);

    VkImportMemoryWin32HandleInfoKHR handleInfo = {
        .sType      = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
        .handle     = handle,
    };

    VkMemoryAllocateInfo info = {
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext           = &handleInfo,
        .allocationSize  = req.size,
        .memoryTypeIndex = typeIndex,
    };

    VkDeviceMemory mem;
    CHECKRE(Vk->AllocateMemory(&info, 0, &mem));

    auto block = std::make_shared<MemoryBlock>(Vk, mem, actualProps, req.size, handle);

    ImportedAllocations[typeIndex].emplace_back(block);

    return block->Allocate(req.size);
}

template <class Resource>
requires(std::is_same_v<Resource, VkBuffer> || std::is_same_v<Resource, VkImage>)
    Allocation VulkanAllocator::AllocateResourceMemoryImpl(Resource resource, u8** mapping)
{
    VkMemoryRequirements             req;
    VkPhysicalDeviceMemoryProperties props;

    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if constexpr (std::is_same_v<Resource, VkBuffer>)
    {
        if (mapping)
        {
            memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        }
        Vk->GetBufferMemoryRequirements(resource, &req);
    }
    else
    {
        Vk->GetImageMemoryRequirements(resource, &req);
    }

    auto [typeIndex, actualProps] = MemoryTypeIndex(req.memoryTypeBits, memProps);

    Allocation allocation = {};

    if (auto it = Allocations.find(typeIndex); it != Allocations.end())
    {
        auto& [_, blocks] = *it;

        for (auto& block : blocks)
        {
            if ((allocation = block->Allocate(req.size)).IsValid())
            {
                break;
            }
        }
    }

    // If the value of VkExportMemoryAllocateInfo::handleTypes used to allocate memory is not 0,
    // it must include at least one of the handles set in VkExternalMemoryBufferCreateInfo::handleTypes when buffer was created

    if (!allocation.IsValid())
    {
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
            // .pNext           = &exportInfo,
            .allocationSize  = std::max(req.size, DefaultChunkSize),
            .memoryTypeIndex = typeIndex,
        };

        VkDeviceMemory mem;
        CHECKRE(Vk->AllocateMemory(&info, 0, &mem));

        auto block = std::make_shared<MemoryBlock>(Vk, mem, actualProps, info.allocationSize);
        allocation = block->Allocate(req.size);
        Allocations[typeIndex].emplace_back(block);
    }

    if (mapping)
    {
        *mapping = allocation.Map();
    }

    if constexpr (std::is_same_v<Resource, VkBuffer>)
    {
        Vk->BindBufferMemory(resource, allocation.Get(), allocation.Offset);
    }
    else
    {
        Vk->BindImageMemory(resource, allocation.Get(), allocation.Offset);
    }

    return allocation;
}
