#pragma once

#include "Allocator.h"

union DescriptorResourceInfo {
    VkDescriptorImageInfo  image;
    VkDescriptorBufferInfo buffer;
};

struct VulkanBuffer : std::enable_shared_from_this<VulkanBuffer>
{

    VulkanDevice* Vk;

    Allocation Allocation;

    VkBuffer Handle;

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

    VulkanBuffer(VulkanDevice* Vk, u64 size, VkBufferUsageFlags usage, bool map);

    VulkanBuffer(VulkanAllocator* Allocator, u64 size, VkBufferUsageFlags usage, bool map);

    ~VulkanBuffer()
    {
        Vk->DestroyBuffer(Handle, 0);
        Allocation.Free();
    }
};
