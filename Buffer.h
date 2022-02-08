#pragma once

#include "Allocator.h"

#include "assert.h"

struct Buffer
{
    enum
    {
        Src     = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        Dst     = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        Vertex  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        Index   = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VkDevice device;

    VkBuffer       handle;
    VkDeviceMemory memory;
    u64            size;

    HANDLE osHandle;

    u8* mapping;

    void Copy(size_t len, void* pp, size_t offset = 0)
    {
        assert(offset + len < this->size);
        memcpy(mapping + offset, pp, len);
    }

    template <class T>
    void Copy(T const& obj, size_t offset = 0)
    {
        assert(offset + sizeof(T) < this->size);
        memcpy(mapping + offset, &obj, sizeof(T));
    }

    void Free()
    {
        vkDestroyBuffer(device, handle, 0);
        vkFreeMemory(device, memory, 0);
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

        vkUpdateDescriptorSets(device, 1, &write, 0, 0);
    }

    size_t Hash()
    {
        return (size_t)handle;
    }

    void Create(Allocator allocator, u64 size, VkBufferUsageFlags usage, bool map)
    {
        device = allocator.device;

        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = usage,
        };

        CHECKRE(vkCreateBuffer(device, &buffer_info, 0, &handle));
        memory = allocator.AllocateBufferMemory(handle, &size);

        mapping = 0;

        if (map)
        {
            CHECKRE(vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, (void**)&mapping));
        }

        VkMemoryGetWin32HandleInfoKHR handleInfo;
        CHECKRE(vkGetMemoryWin32HandleKHR(device, &handleInfo, &osHandle));
    }
};
