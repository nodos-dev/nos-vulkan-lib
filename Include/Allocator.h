#pragma once

#include <mzVkCommon.h>

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
    friend struct MemoryBlock;

  private:
    rc<MemoryBlock> Block;
    VkDeviceSize Offset;
    VkDeviceSize Size;
};

struct mzVulkan_API Allocator : SharedFactory<Allocator>, DeviceChild
{
    static constexpr u64 DefaultChunkSize = 256 * 1024 * 1024;
    struct NativeAPI* Dx;
    std::map<u32, std::vector<rc<MemoryBlock>>> Allocations;
    Allocator(Device* Vk);
    Allocation AllocateResourceMemory(std::variant<VkBuffer, VkImage> resource, bool map = false, const MemoryExportInfo* exported = 0);
    Allocation AllocateImageMemory(VkImage img, VkExtent2D extent, VkFormat format, const MemoryExportInfo* exported = 0);
};
} // namespace mz::vk