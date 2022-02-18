#pragma once

#include "Allocator.h"

union DescriptorResourceInfo {
    VkDescriptorImageInfo  image;
    VkDescriptorBufferInfo buffer;
};

struct VulkanBuffer : std::enable_shared_from_this<VulkanBuffer>
{
    enum
    {
        Src     = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        Dst     = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        Vertex  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        Index   = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VulkanDevice* Vk;
    u8*           Mapping;
    Allocation    Allocation;

    VkBuffer Handle;

    void Copy(size_t len, void* pp, size_t offset = 0)
    {
        assert(offset + len < Allocation.Size);
        memcpy(Mapping + offset, pp, len);
    }

    template <class T>
    void Copy(T const& obj, size_t offset = 0)
    {
        assert(offset + sizeof(T) < Allocation.Size);
        memcpy(Mapping + offset, &obj, sizeof(T));
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

    // VulkanBuffer(std::shared_ptr<VulkanAllocator> allocator, HANDLE osHandle, u64 size, VkBufferUsageFlags usage, bool map)
    //     : Vk(allocator->Vk), handle(allocator->ImportBuffer(osHandle, size, usage)),
    //       allocation(allocator->ImportResourceMemory(handle, osHandle)), osHandle(osHandle), mapping(map ? allocation.Map() : 0)
    // {
    // }

    VulkanBuffer(std::shared_ptr<VulkanAllocator> allocator, u64 size, VkBufferUsageFlags usage, bool map);

    HANDLE GetOSHandle()
    {
        return Allocation.GetOSHandle();
    }

    ~VulkanBuffer()
    {
        Vk->DestroyBuffer(Handle, 0);
        Allocation.Free();
    }
};
