#pragma once

#include "Device.h"

struct MemoryBlock : std::enable_shared_from_this<MemoryBlock>, Uncopyable
{
    struct Allocation
    {
        std::shared_ptr<MemoryBlock> Block;
        VkDeviceSize                 Offset;
        VkDeviceSize                 Size;

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
            if (Block->Mapping)
            {
                return Block->Mapping + Offset;
            }
            return 0;
        }

        void Free()
        {
            if (IsValid() && !Block->IsImported())
            {
                Block->Free(*this);
            }
        }

        VkDeviceMemory Get()
        {
            return Block.get() ? Block->Memory : 0;
        }

        // Will need a platform abstraction here
        HANDLE GetOSHandle()
        {
            if (Block->IsImported())
            {
                return Block->OSHandle;
            }

            VkMemoryGetWin32HandleInfoKHR handleInfo = {
                .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                .memory     = Block->Memory,
                .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            };

            HANDLE handle;
            MZ_VULKAN_ASSERT_SUCCESS(Block->Vk->GetMemoryWin32HandleKHR(&handleInfo, &handle));
            return handle;
        }
    };

    VulkanDevice* Vk;

    VkDeviceMemory        Memory;
    VkMemoryPropertyFlags Props;

    HANDLE OSHandle;
    u8*    Mapping;

    u64 Size;
    u64 InUse;

    std::map<u64, u64> Chunks;
    std::map<u64, u64> FreeList;

    MemoryBlock(VulkanDevice* Vk, VkDeviceMemory mem, VkMemoryPropertyFlags props, u64 size, HANDLE osHandle = 0)
        : Vk(Vk), Memory(mem), Props(props), Size(size), InUse(0), OSHandle(osHandle), Mapping(0)
    {
        FreeList[0] = size;

        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            MZ_VULKAN_ASSERT_SUCCESS(Vk->MapMemory(Memory, 0, VK_WHOLE_SIZE, 0, (void**)&Mapping));
        }
    }

    ~MemoryBlock()
    {
        Vk->FreeMemory(Memory, 0);
    }

    bool IsImported() const
    {
        return OSHandle != nullptr;
    }

    Allocation Allocate(u64 size);

    void Free(Allocation c);
};

using Allocation = MemoryBlock::Allocation;

struct VulkanAllocator : std::enable_shared_from_this<VulkanAllocator>, Uncopyable
{
    static constexpr u64 DefaultChunkSize = 256 * 1024 * 1024;

    VulkanDevice* Vk;

    std::map<u32, std::vector<std::shared_ptr<MemoryBlock>>> Allocations;

    std::map<u32, std::vector<std::shared_ptr<MemoryBlock>>> ImportedAllocations;

    VulkanAllocator(VulkanDevice* Vk)
        : Vk(Vk)
    {
    }

    std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps);

    Allocation ImportResourceMemory(VkBuffer, HANDLE);
    Allocation ImportResourceMemory(VkImage, HANDLE);
    Allocation AllocateResourceMemory(VkBuffer, u8** = 0);
    Allocation AllocateResourceMemory(VkImage, u8** = 0);

  private:
    template <class Resource>
    requires(std::is_same_v<Resource, VkBuffer> || std::is_same_v<Resource, VkImage>)
        Allocation ImportResourceMemoryImpl(Resource resource, HANDLE handle);

    template <class Resource>
    requires(std::is_same_v<Resource, VkBuffer> || std::is_same_v<Resource, VkImage>)
        Allocation AllocateResourceMemoryImpl(Resource resource, u8** mapping = 0);
};
