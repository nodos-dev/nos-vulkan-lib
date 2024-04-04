// Copyright MediaZ AS. All Rights Reserved.


#include "nosVulkan/Buffer.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/Command.h"

namespace nos::vk
{

Buffer::Buffer(Device* Vk, BufferCreateInfo const& info)
	: ResourceBase(Vk), Alignment(info.MemProps.Alignment), Usage(info.Usage),
	  State{.StageMask = VK_PIPELINE_STAGE_2_NONE,
	        .AccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT}, ElementType(info.ElementType)
{
	Size = info.Size;
	Allocation = vk::Allocation{};
	auto type = info.ExternalMemoryHandleType;
	Allocation->MemProps = info.MemProps;
	if (type && info.MemProps.VRAM && info.MemProps.Mapped)
	{
		assert(!"Memory on BAR cannot be bound to external memory");
		Allocation->MemProps.VRAM = false;
	}

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
	if (Allocation->MemProps.VRAM)
		memProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (Allocation->MemProps.Mapped)
		memProps |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	if (auto* imported = info.Imported)
	{
		NOSVK_ASSERT(Vk->CreateBuffer(&bufferCreateInfo, 0, &Handle));
		NOSVK_ASSERT(Allocation->Import(Vk, Handle, *imported, memProps));
	}
	else
	{
		VmaAllocationCreateInfo allocCreateInfo = {
			.flags = Allocation->MemProps.Mapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT : (VmaAllocationCreateFlags)0,
			.usage = (Allocation->MemProps.Download && Allocation->MemProps.Mapped)
																? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
																: VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
												   .requiredFlags = memProps};
		if (Allocation->MemProps.Mapped)
		{
			allocCreateInfo.flags |= Allocation->MemProps.Download
										 ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
															: VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		}
		NOSVK_ASSERT(vmaCreateBufferWithAlignment(Vk->Allocator, &bufferCreateInfo, &allocCreateInfo, Alignment, &Handle, &Allocation->Handle, &Allocation->Info));
	}

	VkMemoryRequirements memReq = {};
	Vk->GetBufferMemoryRequirements(Handle, &memReq);
	assert(memReq.size == Allocation->GetSize());

	NOSVK_ASSERT(Allocation->SetExternalMemoryHandleType(Vk, info.ExternalMemoryHandleType));
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
        .size      = Src->Size,
    };

	if (!Region)
		Region = &DefaultRegion;
	
	Src->Transition(Cmd, BufferMemoryState{.StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .AccessMask = VK_ACCESS_2_TRANSFER_READ_BIT}, Region->srcOffset, Region->size);
	Transition(Cmd, BufferMemoryState{.StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT, .AccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT}, Region->dstOffset, Region->size);
	
    Cmd->CopyBuffer(Src->Handle, this->Handle, 1, Region);
}

void Buffer::Transition(rc<CommandBuffer> cmd, BufferMemoryState dst, VkDeviceSize offset, VkDeviceSize size)
{
	if (Vk->Features.synchronization2)
	{
		VkBufferMemoryBarrier2 barrier {
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask = State.StageMask,
			.srcAccessMask = State.AccessMask,
			.dstStageMask = dst.StageMask,
			.dstAccessMask = dst.AccessMask,
			.buffer = this->Handle,
			.offset = offset,
			.size = size,
		};
		VkDependencyInfo depInfo = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = VK_DEPENDENCY_DEVICE_GROUP_BIT,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &barrier,
		};
		cmd->PipelineBarrier2(&depInfo);
	}
	else
	{
		GLog.E("BufferTransition: Memory barriers are currently only implemented for synchronization2!");
	}
	State = dst;
	cmd->AddDependency(shared_from_this());
}

void Buffer::Copy(size_t len, const void* pp, size_t offset)
{
    assert(offset + len <= Size);
    memcpy(Map() + offset, pp, len);
}

u8* Buffer::Map()
{
	if (!Allocation)
		return nullptr;
    if (Allocation->Imported)
        NOSVK_ASSERT(Vk->MapMemory(Allocation->GetMemory(), Allocation->GetOffset(), Allocation->GetSize(), 0, &Allocation->Mapping()))
    return (u8*)Allocation->Mapping();
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
    if (Allocation)
    {
        if (Allocation->Imported)
		    Vk->DestroyBuffer(Handle, 0);
	    else if (Allocation->Handle)
		    vmaDestroyBuffer(Vk->Allocator, Handle, Allocation->Handle);
    }
}

} // namespace nos::vk