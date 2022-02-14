#pragma once

#define VK_USE_PLATFORM_WIN32_KHR

#include "Device.h"

struct MemoryBlock : std::enable_shared_from_this<MemoryBlock>
{
    struct Allocation
    {
        std::shared_ptr<MemoryBlock> Block;
        u64                          Offset;
        u64                          Size;

        Allocation()
            : Block(0), Offset(0), Size(0)
        {
        }

        Allocation(std::shared_ptr<MemoryBlock> Block, u64 Offset, u64 Size)
            : Block(Block), Offset(Offset), Size(Size)
        {
        }

        bool IsValid() const
        {
            return Block.get() != nullptr;
        }

        void BindBuffer(VkBuffer buffer)
        {
            Block->Vk->BindBufferMemory(buffer, Block->Memory, Offset);
        }

        u8* Map()
        {
            void* p;
            CHECKRE(Block->Vk->MapMemory(Block->Memory, Offset, Size, 0, &p));
            return (u8*)p;
        }

        void Free()
        {
            Block->Free(*this);
        }

        VkDeviceMemory Get() const
        {
            return Block.get() ? Block->Memory : 0;
        }

        // Will need a platform abstraction here
        HANDLE GetOSHandle()
        {
            if (Block->IsImported())
            {
                return Block->osHandle;
            }

            VkMemoryGetWin32HandleInfoKHR handleInfo = {
                .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                .memory     = Block->Memory,
                .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            };
            HANDLE handle;
            CHECKRE(Block->Vk->GetMemoryWin32HandleKHR(&handleInfo, &handle));
            return handle;
        }
    };

    std::shared_ptr<VulkanDevice> Vk;

    VkDeviceMemory        Memory;
    VkMemoryPropertyFlags Props;

    HANDLE osHandle;

    u64 Size;
    u64 InUse;

    std::map<u64, u64> Chunks;
    std::map<u64, u64> FreeList;

    MemoryBlock(std::shared_ptr<VulkanDevice> Vk, VkDeviceMemory mem, VkMemoryPropertyFlags props, u64 size, HANDLE osHandle = 0)
        : Vk(Vk), Memory(mem), Props(props), Size(size), InUse(0), osHandle(osHandle)
    {
        FreeList[0] = size;
    }

    bool IsImported() const
    {
        return osHandle != 0;
    }

    Allocation Allocate(u64 size)
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

        Allocation chunk = {shared_from_this(), next->first, size};

        if (next->second > size)
        {
            FreeList[next->first + size] = next->second - size;
        }

        FreeList.erase(next);

        InUse += size;
        return chunk;
    }

    void Free(Allocation c)
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
            auto next = std::next(it);
            while (next != FreeList.end() && it->first + it->second == next->first)
            {
                it->second += next->second;
                FreeList.erase(next++);
            }
        }
    }
};

using Allocation = MemoryBlock::Allocation;

struct Allocator : std::enable_shared_from_this<Allocator>
{
    std::shared_ptr<VulkanDevice> Vk;

    std::map<u32, std::vector<std::shared_ptr<MemoryBlock>>> Allocations;

    std::map<u32, std::vector<std::shared_ptr<MemoryBlock>>> ImportedAllocations;

    std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps)
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

    VkBuffer ImportBuffer(HANDLE handle, u64 size, VkFlags usage)
    {

        VkMemoryWin32HandlePropertiesKHR props = {.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR};

        CHECKRE(Vk->GetMemoryWin32HandlePropertiesKHR(VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT, handle, &props));

        VkExternalMemoryBufferCreateInfo resourceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,

            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
        };

        VkBufferCreateInfo buffer_info = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext       = &resourceCreateInfo,
            .size        = size,
            .usage       = usage,
            .sharingMode = VK_SHARING_MODE_CONCURRENT,
        };

        VkBuffer buffer;
        CHECKRE(Vk->CreateBuffer(&buffer_info, 0, &buffer));
        return buffer;
    }

    template <class Resource>
    requires(std::is_same_v<Resource, VkBuffer> || std::is_same_v<Resource, VkImage>)
        Allocation ImportResourceMemory(Resource resource, HANDLE handle)
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
        Allocation AllocateResourceMemory(Resource resource, HANDLE* handle, u8** mapping = 0)
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
                .pNext           = &exportInfo,
                .allocationSize  = req.size,
                .memoryTypeIndex = typeIndex,
            };

            VkDeviceMemory mem;
            CHECKRE(Vk->AllocateMemory(&info, 0, &mem));

            auto block = std::make_shared<MemoryBlock>(Vk, mem, actualProps, req.size);
            allocation = block->Allocate(req.size);
            Allocations[typeIndex].emplace_back(block);
        }

        if (mapping)
        {
            *mapping = allocation.Map();
        }

        if (handle)
        {
            *handle = allocation.GetOSHandle();
        }

        return allocation;
    }
};
