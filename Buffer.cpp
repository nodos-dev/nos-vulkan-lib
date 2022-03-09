#include "Buffer.h"

namespace mz::vk
{
Buffer::Buffer(Allocator* Allocator, u64 size, VkBufferUsageFlags usage)
    : Vk(Allocator->GetDevice()), Usage(usage)
{

    VkExternalMemoryBufferCreateInfo resourceCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };

    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &resourceCreateInfo,
        .size  = size,
        .usage = usage,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateBuffer(&info, 0, &Handle));

    Allocation = Allocator->AllocateResourceMemory(Handle, usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));

    Vk->BindBufferMemory(Handle, Allocation.Block->Memory, Allocation.Offset + Allocation.Block->Offset);
}

Buffer::Buffer(Device* Vk, u64 size, VkBufferUsageFlags usage)
    : Buffer(Vk->ImmAllocator.get(), size, usage)
{
}

void Buffer::Bind(VkDescriptorType type, u32 bind, VkDescriptorSet set)
{
    VkDescriptorBufferInfo info = {
        .buffer = Handle,
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
} // namespace mz::vk