#pragma once

#include "Device.h"

#include "InfoStructs.h"

namespace mz::vk
{

struct mzVulkan_API MemoryBlock : SharedFactory<MemoryBlock>
{
    struct mzVulkan_API Allocation
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

        u8* Map()
        {
            if (Block->Mapping)
            {
                return Block->Mapping + Offset + Block->Offset;
            }
            return 0;
        }

        void Flush()
        {
            VkMappedMemoryRange range = {
                .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = Block->Memory,
                .offset = Offset + Block->Offset,
                .size   = Size,
            };

            MZ_VULKAN_ASSERT_SUCCESS(Block->Vk->FlushMappedMemoryRanges(1, &range));
        }

        HANDLE GetOSHandle() const
        {
            assert(Block->OSHandle);

            return Block->OSHandle;
        }

        void Free()
        {
            if (IsValid())
            {
                Block->Free(*this);
            }
        }

        void BindResource(VkImage image)
        {
            MZ_VULKAN_ASSERT_SUCCESS(Block->Vk->BindImageMemory(image, Block->Memory, Offset + Block->Offset));
        }

        void BindResource(VkBuffer buffer)
        {
            MZ_VULKAN_ASSERT_SUCCESS(Block->Vk->BindBufferMemory(buffer, Block->Memory, Offset + Block->Offset));
        }
    };

    Device* Vk;

    VkDeviceMemory        Memory;
    VkMemoryPropertyFlags Props;

    HANDLE OSHandle;
    u8*    Mapping;

    const bool Imported;

    u64 Offset;
    u64 Size;
    u64 InUse;

    std::map<VkDeviceSize, VkDeviceSize> Chunks;
    std::map<VkDeviceSize, VkDeviceSize> FreeList;

    MemoryBlock(Device* Vk, VkDeviceMemory mem, VkMemoryPropertyFlags props, u64 offset, u64 size, HANDLE externalHandle)
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

    ~MemoryBlock()
    {

        Vk->FreeMemory(Memory, 0);

        // Imported blocks do not need to decrease the refcount
        if (!Imported)
        {
            assert(SUCCEEDED(CloseHandle(OSHandle)));
        }
    }

    Allocation Allocate(VkDeviceSize size, VkDeviceSize alignment);

    void Free(Allocation c);
};

using Allocation = MemoryBlock::Allocation;

std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(VkPhysicalDevice physicalDevice, u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps);

struct mzVulkan_API Allocator : SharedFactory<Allocator>
{
    static constexpr u64 DefaultChunkSize = 256 * 1024 * 1024;

    Device* Vk;

    std::map<u32, std::vector<std::shared_ptr<MemoryBlock>>> Allocations;

    Allocator(Device* Vk);

    Device* GetDevice()
    {
        return Vk;
    }

    Allocation AllocateResourceMemory(std::variant<VkBuffer, VkImage> resource, bool map = false, const ImageExportInfo* exported = 0);
};
} // namespace mz::vk