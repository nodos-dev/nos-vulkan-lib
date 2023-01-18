/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Common.h"

namespace mz::vk
{
struct MemoryBlock;
struct Device;

struct mzVulkan_API Allocation
{
    Allocation();
    Allocation(rc<MemoryBlock> Block, u64 Offset, u64 Size);
    bool IsValid() const;
    u8* Map();
    void Flush();
    void Free();
    void BindResource(VkImage);
    void BindResource(VkBuffer);
    HANDLE GetOSHandle() const;
    VkDeviceSize GlobalOffset() const;
    VkDeviceSize LocalOffset() const;
    VkDeviceSize LocalSize() const;
    VkDeviceSize GlobalSize() const;
    VkDeviceMemory GetMemory() const;
    VkExternalMemoryHandleTypeFlagBits GetType() const;
    friend struct MemoryBlock;

  private:
    rc<MemoryBlock> Block;
    VkDeviceSize Offset;
    VkDeviceSize Size;
};

struct mzVulkan_API Allocator : SharedFactory<Allocator>, DeviceChild
{
    std::mutex Mutex;
    static constexpr u64 DefaultChunkSize = 256 * 1024 * 1024;
    struct NativeAPI* Dx;
    std::map<u32, std::set<MemoryBlock*>> Allocations;
    Allocator(Device* Vk);

    Allocation AllocateResourceMemory(std::variant<VkBuffer, VkImage> resource, VkExternalMemoryHandleTypeFlagBits type, bool map, bool vram, const MemoryExportInfo* exported);

    Allocation AllocateImageMemory(VkImage img, ImageCreateInfo const& info);
};
} // namespace mz::vk