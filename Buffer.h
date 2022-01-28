#pragma once

#include "Common.h"

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
    
    enum Mapping
    {
        Mapped,
        Unmapped,
    };

    VkBuffer      handle;
    u8*           mapping;
    VmaAllocation allocation;


    void unmapped(struct RenderResources& res, vector<pair<u64, u8*>> raws, VkBufferUsageFlags usage = Buffer::Vertex | Buffer::Index);


    void bind_vertex(u64 offset, u32 binding, VkCommandBuffer cmd)
    {
        vkCmdBindVertexBuffers(cmd, binding, 1, &handle, &offset);
    }

    void bind_index(u64 offset, VkCommandBuffer cmd)
    {
        vkCmdBindIndexBuffer(cmd, handle, offset, VK_INDEX_TYPE_UINT32);
    }

    void copy(size_t len, void* pp, size_t offset)
    {
        memcpy(mapping + offset, pp, len);
    }

    template <class T>
    void copy(T const& obj, size_t offset)
    {
        memcpy(mapping + offset, &obj, sizeof(T));
    }

    void free(VmaAllocator allocator)
    {
        vmaDestroyBuffer(allocator, handle, allocation);
    }

    void bind_to_set(VkDevice dev, VkDescriptorType type, u32 bind, VkDescriptorSet set)
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

        vkUpdateDescriptorSets(dev, 1, &write, 0, 0);
    }

    size_t hash()
    {
        return (size_t)handle;
    }

    void create(VmaAllocator allocator, u64 size, VkBufferUsageFlags usage, Mapping map)
    {
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = size,
            .usage = usage};

        VmaAllocationCreateInfo allocation_info;
        switch (map)
        {
        case Mapped:
            allocation_info = {
                .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            };
            break;
        case Unmapped:
            allocation_info = {
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            };
            break;
        }
        VmaAllocationInfo alloc_info;
        CHECKRE(vmaCreateBuffer(allocator, &buffer_info, &allocation_info, &handle, &allocation, &alloc_info));

        mapping = (u8*)alloc_info.pMappedData;
    }
};
