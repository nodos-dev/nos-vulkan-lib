#pragma once

#include <Allocator.h>

namespace mz::vk
{

struct mzVulkan_API Buffer : SharedFactory<Buffer>
{
    Device* Vk;

    Allocation Allocation;

    VkBuffer Handle;

    VkBufferUsageFlags Usage;

    void Copy(size_t len, void* pp, size_t offset = 0);

    template <class T>
    void Copy(T const& obj, size_t offset = 0)
    {
        Copy(sizeof(T), &obj, offset);
    }

    u8* Map();

    void Flush();

    void Bind(VkDescriptorType type, u32 bind, VkDescriptorSet set);

    DescriptorResourceInfo GetDescriptorInfo() const;

    enum Heap
    {
        GPU,
        CPU,
    };

    Buffer(Device* Vk, u64 size, VkBufferUsageFlags usage, Heap heap);
    Buffer(Allocator* Allocator, u64 size, VkBufferUsageFlags usage, Heap heap);
    Buffer(Allocator* Allocator, u8* data, u64 size, VkBufferUsageFlags usage);

    ~Buffer();

    void Upload(rc<CommandBuffer> Cmd, rc<Buffer> Buffer, const VkBufferCopy* Region = 0);
};

} // namespace mz::vk