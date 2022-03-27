
#include <Buffer.h>

#include <Command.h>

namespace mz::vk
{
Buffer::Buffer(Allocator* Allocator, u64 size, VkBufferUsageFlags usage, Heap heap)
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

    Allocation = Allocator->AllocateResourceMemory(Handle, heap == CPU);

    Allocation.BindResource(Handle);
}

Buffer::Buffer(Device* Vk, u64 size, VkBufferUsageFlags usage, Heap heap)
    : Buffer(Vk->ImmAllocator.get(), size, usage, heap)
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

void Buffer::Upload(u8* data, Allocator* Allocator, CommandPool* Pool)
{
    // if this buffer has already been mapped you could simply use the mapped pointer instead of creating a temporary buffer
    assert(!Map());
    assert(Usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    if (0 == Allocator)
    {
        Allocator = Vk->ImmAllocator.get();
    }

    u64 Size = Allocation.Size;

    rc<Buffer> StagingBuffer = Buffer::New(Allocator, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, CPU);

    memcpy(StagingBuffer->Map(), data, Size);
    StagingBuffer->Flush();
    Upload(StagingBuffer, Pool);
}

void Buffer::Upload(rc<Buffer> buffer, CommandPool* Pool)
{
    // if this buffer has already been mapped you could simply use the mapped pointer instead of creating a temporary buffer
    assert(!Map());
    assert(Usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    if (0 == Pool)
    {
        Pool = Vk->ImmCmdPool.get();
    }

    rc<CommandBuffer> Cmd = Pool->BeginCmd();

    VkBufferMemoryBarrier bufferMemoryBarrier = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .buffer = this->Handle,
        .size   = 1024,
    };

    Cmd->PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1, &bufferMemoryBarrier, 0, 0);

    VkBufferCopy region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = buffer->Allocation.Size,
    };

    Cmd->CopyBuffer(buffer->Handle, this->Handle, 1, &region);
    Cmd->Submit({}, {}, {});
    Cmd->Wait();
}

void Buffer::Copy(size_t len, void* pp, size_t offset)
{
    assert(offset + len < Allocation.Size);
    memcpy(Allocation.Map() + offset, pp, len);
}

u8* Buffer::Map()
{
    return Allocation.Map();
}

void Buffer::Flush()
{
    Allocation.Flush();
}

HANDLE Buffer::GetOSHandle()
{
    return Allocation.GetOSHandle();
}

DescriptorResourceInfo Buffer::GetDescriptorInfo() const
{
    return DescriptorResourceInfo{
        .buffer = {
            .buffer = Handle,
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        }};
}

Buffer::~Buffer()
{
    Vk->DestroyBuffer(Handle, 0);
    Allocation.Free();
}

} // namespace mz::vk