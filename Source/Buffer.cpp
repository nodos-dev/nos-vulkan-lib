// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


#include "nosVulkan/Buffer.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/Command.h"

namespace nos::vk
{

Buffer::Buffer(Device* Vk, BufferCreateInfo const& info) 
    : ResourceBase(Vk), Usage(info.Usage)
{
	auto type = info.ExternalMemoryHandleType;

	VkExternalMemoryBufferCreateInfo resourceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
		.handleTypes = type,
	};

	VkBufferCreateInfo bufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = type ? &resourceCreateInfo : 0,
		.size = info.Size,
		.usage = info.Usage,
	};

	VkMemoryPropertyFlags memProps = 0;
	if (info.Mapped)
		memProps |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	if (info.VRAM)
		memProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (auto* imported = info.Imported)
	{
		NOSVK_ASSERT(Vk->CreateBuffer(&bufferCreateInfo, 0, &Handle));
        NOSVK_ASSERT(Allocation.Import(Vk, Handle, *imported, memProps));
	}
	else
	{
		VmaAllocationCreateInfo allocCreateInfo = {
			.flags = info.Mapped ? VmaAllocationCreateFlags(
                VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
				VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | 
                VMA_ALLOCATION_CREATE_MAPPED_BIT) : 0,
			.usage = VMA_MEMORY_USAGE_AUTO, 
			.requiredFlags = memProps
		};
		NOSVK_ASSERT(vmaCreateBuffer(Vk->Allocator, &bufferCreateInfo, &allocCreateInfo, &Handle, &Allocation.Handle, &Allocation.Info));
	}

	VkMemoryRequirements memReq = {};
	Vk->GetBufferMemoryRequirements(Handle, &memReq);
	assert(memReq.size == Allocation.GetSize());

	NOSVK_ASSERT(Allocation.SetExternalMemoryHandleType(Vk, info.ExternalMemoryHandleType));
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
        .size      = Src->Allocation.GetSize(),
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

void Buffer::Copy(size_t len, const void* pp, size_t offset)
{
    assert(offset + len <= Allocation.GetSize());
    memcpy(Map() + offset, pp, len);
}

u8* Buffer::Map()
{
    if (Allocation.Imported)
        NOSVK_ASSERT(Vk->MapMemory(Allocation.GetMemory(), Allocation.GetOffset(), Allocation.GetSize(), 0, &Allocation.Mapping()))
    return (u8*)Allocation.Mapping();
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
	if (Allocation.Imported)
		Vk->DestroyBuffer(Handle, 0);
	else if (Allocation.Handle)
		vmaDestroyBuffer(Vk->Allocator, Handle, Allocation.Handle);
}

} // namespace nos::vk