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

        bool IsImported() const
        {
            return Block->Imported;
        }

        template <class Resource>
        requires(std::is_same_v<Resource, VkBuffer> || std::is_same_v<Resource, VkImage>) void BindResource(Resource resource)
        {
            if constexpr (std::is_same_v<Resource, VkBuffer>)
            {
                Block->Vk->BindBufferMemory(resource, Block->Memory, Offset);
            }
            else
            {
                Block->Vk->BindImageMemory(resource, Block->Memory, Offset);
            }
        }

        u8* Map()
        {
            if (Block->Mapping)
            {
                return Block->Mapping + Offset;
            }
            return 0;
        }

        HANDLE GetOSHandle() const
        {
            return Block->OSHandle;
        }

        void Free()
        {
            if (IsValid())
            {
                Block->Free(*this);
            }
        }
    };

    VulkanDevice* Vk;

    VkDeviceMemory        Memory;
    VkMemoryPropertyFlags Props;

    HANDLE OSHandle;
    u8*    Mapping;

    const bool Imported;

    u64 Size;
    u64 InUse;

    std::map<VkDeviceSize, VkDeviceSize> Chunks;
    std::map<VkDeviceSize, VkDeviceSize> FreeList;

    MemoryBlock(VulkanDevice* Vk, VkDeviceMemory mem, VkMemoryPropertyFlags props, u64 size, HANDLE externalHandle)
        : Vk(Vk), Memory(mem), Props(props), Size(size), InUse(0), OSHandle(externalHandle), Mapping(0), Imported(externalHandle != 0)
    {
        FreeList[0] = size;

        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            MZ_VULKAN_ASSERT_SUCCESS(Vk->MapMemory(Memory, 0, VK_WHOLE_SIZE, 0, (void**)&Mapping));
        }

        if (!Imported)
        {
            VkMemoryGetWin32HandleInfoKHR handleInfo = {
                .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                .memory     = Memory,
                .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            };

            MZ_VULKAN_ASSERT_SUCCESS(Vk->GetMemoryWin32HandleKHR(&handleInfo, &OSHandle));
        }
    }

    ~MemoryBlock()
    {
        Vk->FreeMemory(Memory, 0);
        assert(SUCCEEDED(CloseHandle(OSHandle)));
    }

    Allocation Allocate(VkDeviceSize size, VkDeviceSize alignment);

    void Free(Allocation c);
};

using Allocation = MemoryBlock::Allocation;

std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(VkPhysicalDevice physicalDevice, u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps);

struct VulkanAllocator : std::enable_shared_from_this<VulkanAllocator>, Uncopyable
{
    static constexpr u64 DefaultChunkSize = 256 * 1024 * 1024;

    VulkanDevice* Vk;

    std::map<u32, std::vector<std::shared_ptr<MemoryBlock>>> Allocations;

    VulkanAllocator(VulkanDevice* Vk)
        : Vk(Vk)
    {
    }

    Allocation AllocateResourceMemory(VkBuffer resource, bool map = false, HANDLE externalHandle = 0);
    Allocation AllocateResourceMemory(VkImage resource, HANDLE externalHandle = 0);

  private:
    template <class Resource>
    requires(std::is_same_v<Resource, VkBuffer> || std::is_same_v<Resource, VkImage>)
        Allocation AllocateResourceMemoryImpl(Resource resource, bool map = false, HANDLE externalHandle = 0);
};
