#pragma once

#include <Device.h>

namespace mz::vk
{

struct mzVulkan_API MemoryBlock : SharedFactory<MemoryBlock>
{
    struct mzVulkan_API Allocation
    {
        rc<MemoryBlock> Block;
        VkDeviceSize Offset;
        VkDeviceSize Size;

        Allocation();
        Allocation(rc<MemoryBlock> Block, u64 Offset, u64 Size);
        bool IsValid() const;
        u8* Map();
        void Flush();
        void Free();
        void BindResource(VkImage image);
        void BindResource(VkBuffer buffer);
    };

    Device* Vk;

    VkDeviceMemory Memory;
    VkMemoryPropertyFlags Props;

    HANDLE OSHandle;
    u8* Mapping;

    u64 Offset;
    u64 Size;
    u64 InUse;

    std::map<VkDeviceSize, VkDeviceSize> Chunks;
    std::map<VkDeviceSize, VkDeviceSize> FreeList;

    MemoryBlock(Device* Vk, VkDeviceMemory mem, VkMemoryPropertyFlags props, u64 offset, u64 size, HANDLE OSHandle);

    ~MemoryBlock();

    Allocation Allocate(VkDeviceSize size, VkDeviceSize alignment);

    void Free(Allocation c);
};

using Allocation = MemoryBlock::Allocation;

struct mzVulkan_API Allocator : SharedFactory<Allocator>
{
    static constexpr u64 DefaultChunkSize = 256 * 1024 * 1024;

    Device* Vk;

    Device* GetDevice() const;

    std::map<u32, std::vector<rc<MemoryBlock>>> Allocations;

    Allocator(Device* Vk);

    Allocation AllocateResourceMemory(std::variant<VkBuffer, VkImage> resource, bool map = false, const MemoryExportInfo* exported = 0);
};
} // namespace mz::vk