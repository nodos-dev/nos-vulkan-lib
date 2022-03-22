#pragma once

#include "Allocator.h"

namespace mz::vk
{

union DescriptorResourceInfo {
    VkDescriptorImageInfo  image;
    VkDescriptorBufferInfo buffer;
};

struct Buffer : SharedFactory<Buffer>
{
    Device* Vk;

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

    enum Heap
    {
        GPU,
        CPU,
    };

    Buffer(Device* Vk, u64 size, VkBufferUsageFlags usage, Heap heap);

    Buffer(Allocator* Allocator, u64 size, VkBufferUsageFlags usage, Heap heap);

    ~Buffer()
    {
        Vk->DestroyBuffer(Handle, 0);
        Allocation.Free();
    }

    void Upload(u8* data, Allocator* = 0, CommandPool* = 0);
    void Upload(std::shared_ptr<Buffer>, CommandPool* = 0);
};

template <class T>
concept TypeClassBuffer = (std::is_same_v<T, Buffer*> || std::is_same_v<T, std::shared_ptr<Buffer>>);

} // namespace mz::vk