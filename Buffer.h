#pragma once

#include "Allocator.h"

namespace mz
{

union DescriptorResourceInfo {
    VkDescriptorImageInfo  image;
    VkDescriptorBufferInfo buffer;
};

struct VulkanBuffer : SharedFactory<VulkanBuffer>
{
    VulkanDevice* Vk;

    Allocation Allocation;

    VkBuffer Handle;

    VkBufferUsageFlags Usage;

    void Copy(size_t len, void* pp, size_t offset = 0)
    {
        assert(offset + len < Allocation.Size);
        memcpy(Allocation.Map() + offset, pp, len);
    }

    template <class T>
    void Copy(T const& obj, size_t offset = 0)
    {
        assert(offset + sizeof(T) < Allocation.Size);
        memcpy(Allocation.Map() + offset, &obj, sizeof(T));
    }

    u8* Map()
    {
        return Allocation.Map();
    }

    void Flush()
    {
        Allocation.Flush();
    }

    HANDLE GetOSHandle()
    {
        return Allocation.GetOSHandle();
    }

    void Bind(VkDescriptorType type, u32 bind, VkDescriptorSet set);

    DescriptorResourceInfo GetDescriptorInfo() const
    {
        return DescriptorResourceInfo{
            .buffer = {
                .buffer = Handle,
                .offset = 0,
                .range  = VK_WHOLE_SIZE,
            }};
    }

    VulkanBuffer(VulkanDevice* Vk, u64 size, VkBufferUsageFlags usage);

    VulkanBuffer(VulkanAllocator* Allocator, u64 size, VkBufferUsageFlags usage);

    ~VulkanBuffer()
    {
        Vk->DestroyBuffer(Handle, 0);
        Allocation.Free();
    }
};
} // namespace mz