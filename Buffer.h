#pragma once

#include "Allocator.h"

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
    VkBuffer      handle;
    Allocation    allocation;
    u8*           mapping;

    void Copy(size_t len, void* pp, size_t offset = 0)
    {
        assert(offset + len < allocation.Size);
        memcpy(mapping + offset, pp, len);
    }

    template <class T>
    void Copy(T const& obj, size_t offset = 0)
    {
        assert(offset + sizeof(T) < allocation.Size);
        memcpy(mapping + offset, &obj, sizeof(T));
    }

    void Bind(VkDescriptorType type, u32 bind, VkDescriptorSet set)
    {
        VkDescriptorBufferInfo info = {
            .buffer = handle,
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet write = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = bind,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pBufferInfo     = &info,
        };

        Vk->UpdateDescriptorSets(1, &write, 0, 0);
    }

    size_t Hash()
    {
        return (size_t)handle;
    }

    // VulkanBuffer(std::shared_ptr<VulkanAllocator> allocator, HANDLE osHandle, u64 size, VkBufferUsageFlags usage, bool map)
    //     : Vk(allocator->Vk), handle(allocator->ImportBuffer(osHandle, size, usage)),
    //       allocation(allocator->ImportResourceMemory(handle, osHandle)), osHandle(osHandle), mapping(map ? allocation.Map() : 0)
    // {
    // }

    VulkanBuffer(std::shared_ptr<VulkanAllocator> allocator, u64 size, VkBufferUsageFlags usage, bool map)
        : Vk(allocator->Vk), mapping(0)
    {

        VkExternalMemoryBufferCreateInfo resourceCreateInfo = {
            .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
        };

        VkBufferCreateInfo info = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext       = &resourceCreateInfo,
            .size        = size,
            .usage       = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        CHECKRE(Vk->CreateBuffer(&info, 0, &handle));

        allocation = allocator->AllocateResourceMemory(handle, map ? &mapping : 0);
    }

    HANDLE GetOSHandle()
    {
        return allocation.GetOSHandle();
    }

    ~VulkanBuffer()
    {
        Vk->DestroyBuffer(handle, 0);
        allocation.Free();
    }
};
