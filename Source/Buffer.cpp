// // Copyright MediaZ AS. All Rights Reserved.


#include "mzVulkan/Buffer.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/Command.h"

namespace mz::vk
{

Buffer::Buffer(Device* Vk, BufferCreateInfo const& info) 
    : Buffer(Vk->ImmAllocator.get(), info)
{
}

Buffer::Buffer(Allocator* Allocator, BufferCreateInfo const& info)
    : DeviceChild(Allocator->GetDevice()), Usage(info.Usage)
{
    if(0 == info.Size)
    {
        UNREACHABLE;
    }

    VkExternalMemoryBufferCreateInfo resourceCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
        .handleTypes = (VkFlags)info.Type,
    };

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = &resourceCreateInfo,
        .size  = info.Size,
        .usage = info.Usage,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateBuffer(&createInfo, 0, &Handle));

    Allocation = Allocator->AllocateResourceMemory(Handle, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT, !info.VRAM, info.Imported);
    Allocation.BindResource(Handle);

    if(info.Data)
    {
        Copy(info.Size, info.Data);
        Allocation.Flush();
    }
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


void Buffer::Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src, const VkBufferCopy* Region)
{
    // if this buffer has already been mapped you could simply use the mapped pointer instead of creating a temporary buffer

    // if (auto dst = Map())
    // {
    //     if (auto src = Src->Map())
    //     {
    //         memcpy(dst, src, Allocation.LocalSize());
    //         return;
    //     }
    //     UNREACHABLE;
    // }

    assert(Usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    assert(Src->Usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    VkBufferCopy DefaultRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = Src->Allocation.LocalSize(),
    };

    if (!Region)
    {
        Region = &DefaultRegion;
    }

    VkBufferMemoryBarrier bufferMemoryBarrier = {
        .sType  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .buffer = this->Handle,
        .size   = Region->size,
    };

    Cmd->PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1, &bufferMemoryBarrier, 0, 0);

    Cmd->CopyBuffer(Src->Handle, this->Handle, 1, Region);

    // make sure the buffers are alive until after the command buffer has finished
    Cmd->AddDependency(Src, shared_from_this());
}

void Buffer::Copy(size_t len, void* pp, size_t offset)
{
    assert(offset + len <= Allocation.LocalSize());
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

DescriptorResourceInfo Buffer::GetDescriptorInfo() const
{
    return DescriptorResourceInfo{
        .Buffer = {
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