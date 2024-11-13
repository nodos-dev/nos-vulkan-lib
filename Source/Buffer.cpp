// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


#include "nosVulkan/Buffer.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/Command.h"

namespace nos::vk
{

BufferCreationInfos::BufferCreationInfos(BufferCreationInfos&& o) noexcept
{
	this->MemoryTypeIndex = o.MemoryTypeIndex;
	this->ExtMemHandleType = o.ExtMemHandleType;
	this->BufCreateInfo = o.BufCreateInfo;
	this->AllocCreateInfo = o.AllocCreateInfo;
	this->MemProps = o.MemProps;
	this->ExtMemCreateInfo = o.ExtMemCreateInfo;
	if (BufCreateInfo.pNext)
		this->BufCreateInfo.pNext = &ExtMemCreateInfo;
}

BufferCreateInfo GetBufferCreateRequestForTempUploadBuffer(uint64_t size)
{
	return vk::BufferCreateInfo{
		.Size = size,
		.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.MemProps = {.Mapped = true, .VRAM = false, .Download = false,},
		.ExternalMemoryHandleType = 0,
		.Temporary = true
	};
}

BufferCreationInfos CalculateBufferCreationInfos(vk::Device* device, BufferCreateInfo const& info)
{
	BufferCreationInfos ret;
	auto& extMemHandleType = (ret.ExtMemHandleType = info.ExternalMemoryHandleType);
	auto& requestedMemProps = info.MemProps;
	auto& extMemCreateInfo = ret.ExtMemCreateInfo;
	auto& bufferCreateInfo = ret.BufCreateInfo;
	auto& allocCreateInfo = ret.AllocCreateInfo;

	extMemCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
		.handleTypes = extMemHandleType,
	};

	bufferCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = extMemHandleType ? &extMemCreateInfo : nullptr,
		.size = info.Size,
		.usage = info.Usage,
	};

	auto& memProps = (ret.MemProps = 0);
	if (requestedMemProps.VRAM)
		memProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (requestedMemProps.Mapped)
		memProps |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	if (info.Imported)
		return ret;
	else
	{
		allocCreateInfo = {
			.flags = requestedMemProps.Mapped ? VMA_ALLOCATION_CREATE_MAPPED_BIT : (VmaAllocationCreateFlags)0,
			.usage = (requestedMemProps.Download && requestedMemProps.Mapped)
																? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
																: VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
			.requiredFlags = memProps,
		};
		if (requestedMemProps.Mapped)
		{
			allocCreateInfo.flags |= requestedMemProps.Download ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
																: VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		}

		auto& memoryTypeIndex = (ret.MemoryTypeIndex = UINT32_MAX);
		auto res = vmaFindMemoryTypeIndexForBufferInfo(device->Allocator, &bufferCreateInfo, &allocCreateInfo, &memoryTypeIndex);
		if (res != VK_SUCCESS)
		{
			if (extMemHandleType)
			{
				GLog.W("Failed to find memory type index for buffer, trying again without external memory handle type");
				bufferCreateInfo.pNext = nullptr;
				extMemHandleType = 0;
				res = vmaFindMemoryTypeIndexForBufferInfo(device->Allocator, &bufferCreateInfo, &allocCreateInfo, &memoryTypeIndex);
			}
			if (res != VK_SUCCESS)
			{
				GLog.E("Failed to find memory type index for buffer");
				return ret;
			}
		}
		return ret;
	}
}


Buffer::Buffer(Device* device, BufferCreateInfo const& info)
	: ResourceBase(device), Alignment(info.MemProps.Alignment), Usage(info.Usage),
	  State{.StageMask = VK_PIPELINE_STAGE_2_NONE,
	        .AccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT}, ElementType(info.ElementType)
{
	Size = info.Size;
	AllocationInfo = vk::Allocation{};
	auto bcInfos = CalculateBufferCreationInfos(device, info);
	AllocationInfo->MemProps = info.MemProps;

	if (auto* imported = info.Imported)
	{
		NOSVK_ASSERT(device->CreateBuffer(&bcInfos.BufCreateInfo, 0, &Handle));
		NOSVK_ASSERT(AllocationInfo->Import(device, Handle, *imported, bcInfos.MemProps));
	}
	else
	{
		if (bcInfos.MemoryTypeIndex != UINT32_MAX && info.Temporary && info.Size < vk::Device::TEMP_MEMORY_POOL_BLOCK_SIZE)
		{
			auto it = device->TempMemoryPools.find(bcInfos.MemoryTypeIndex);
			if (it != device->TempMemoryPools.end())
				bcInfos.AllocCreateInfo.pool = it->second;
		}
		NOSVK_ASSERT(vmaCreateBufferWithAlignment(device->Allocator, &bcInfos.BufCreateInfo, &bcInfos.AllocCreateInfo, Alignment, &Handle, &AllocationInfo->Handle, &AllocationInfo->Info));
	}

	VkMemoryRequirements memReq = {};
	device->GetBufferMemoryRequirements(Handle, &memReq);
	assert(memReq.size == AllocationInfo->GetSize());

	if (bcInfos.ExtMemHandleType || info.Imported)
		NOSVK_ASSERT(AllocationInfo->SetExternalMemoryHandleType(device, info.ExternalMemoryHandleType));
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
	if (!AllocationInfo)
		return nullptr;
    if (AllocationInfo->Imported)
        NOSVK_ASSERT(Vk->MapMemory(AllocationInfo->GetMemory(), AllocationInfo->GetOffset(), AllocationInfo->GetSize(), 0, &AllocationInfo->Mapping()))
    return (u8*)AllocationInfo->Mapping();
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
    if (AllocationInfo)
    {
        if (AllocationInfo->Imported)
		    Vk->DestroyBuffer(Handle, 0);
	    else if (AllocationInfo->Handle)
		    vmaDestroyBuffer(Vk->Allocator, Handle, AllocationInfo->Handle);
    }
}

} // namespace nos::vk