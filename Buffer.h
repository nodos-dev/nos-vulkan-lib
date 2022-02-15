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

    std::shared_ptr<VulkanDevice> Vk;
    VkBuffer                      handle;
    Allocation                    allocation;

    HANDLE osHandle;
    u8*    mapping;

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

    void Free()
    {
        Vk->DestroyBuffer(handle, 0);
        allocation.Free();
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

    VulkanBuffer(std::shared_ptr<VulkanAllocator> allocator, HANDLE osHandle, u64 size, VkBufferUsageFlags usage, bool map)
        : Vk(allocator->Vk), handle(allocator->ImportBuffer(osHandle, size, usage)),
          allocation(allocator->ImportResourceMemory(handle, osHandle)), osHandle(osHandle), mapping(map ? allocation.Map() : 0)
    {
    }

    VulkanBuffer(std::shared_ptr<VulkanAllocator> allocator, u64 size, VkBufferUsageFlags usage, bool map)
        : Vk(allocator->Vk), osHandle(0), mapping(0)
    {
        VkBufferCreateInfo buffer_info = {
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = size,
            .usage       = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        CHECKRE(Vk->CreateBuffer(&buffer_info, 0, &handle));

        allocation = allocator->AllocateResourceMemory(handle, &osHandle, map ? &mapping : 0);
    }

    ~VulkanBuffer()
    {
        Vk->DestroyBuffer(handle, 0);
        allocation.Free();
    }
};
