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

BufferCreateRequest GetBufferCreateRequestForTempUploadBuffer(uint64_t size)
{
	return vk::BufferCreateRequest{
		.Size = size,
		.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.MemProps = {.Mapped = true, .VRAM = false, .Download = false,},
		.ExternalMemoryHandleType = 0,
		.Temporary = true
	};
}

Result<BufferCreationInfos> CalculateBufferCreationInfos(vk::Device* device, BufferCreateRequest const& info)
{
	BufferCreationInfos ret;
	auto& extMemHandleType = (ret.ExtMemHandleType = info.ExternalMemoryHandleType);
	auto requestedMemProps = info.MemProps;
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

	if(requestedMemProps.VRAM && requestedMemProps.ForceHostMemory)
		return "Buffer requested to be created in VRAM but also forced to be created in host memory.";

	auto& memProps = (ret.MemProps = 0);
	if (requestedMemProps.VRAM)
		memProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (requestedMemProps.Mapped)
		memProps |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	if (info.Imported)
		return ret;
	allocCreateInfo = {
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = memProps,
	};
	if (requestedMemProps.Mapped)
	{
		allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
		allocCreateInfo.flags |= requestedMemProps.Download ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
															: VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	if(requestedMemProps.ForceHostMemory || (requestedMemProps.Download && requestedMemProps.Mapped))
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	auto& memoryTypeIndex = (ret.MemoryTypeIndex = UINT32_MAX);
	auto res = vmaFindMemoryTypeIndexForBufferInfo(device->Allocator, &bufferCreateInfo, &allocCreateInfo, &memoryTypeIndex);
	if (res != VK_SUCCESS)
	{
		if (extMemHandleType)
			return "Failed to find memory type index for buffer, maybe try again without exporting.";
		return "Failed to find memory type index for buffer.";
	}
	return ret;
}


Result<rc<Buffer>> Buffer::Create(Device* device, BufferCreateRequest const& info, VkResult* outVkRes)
{
	VkResult res{};
	if (!outVkRes)
		outVkRes = &res;
	// Might return error without vulkan failure
	*outVkRes = VK_SUCCESS;

	auto allocationInfo = vk::Allocation{};
	auto bcInfosRes = CalculateBufferCreationInfos(device, info);
	if (auto* err = bcInfosRes.Error())
		return std::move(*err);
	
	auto& bcInfos = *bcInfosRes.Get();
	allocationInfo.MemProps = info.MemProps;

	VkBuffer handle{};
	if (auto* imported = info.Imported)
	{
		if (NOS_VULKAN_FAILED(*outVkRes = device->CreateBuffer(&bcInfos.BufCreateInfo, 0, &handle)))
		{
			return "Error while creating imported buffer.";
		}
		if (NOS_VULKAN_FAILED(*outVkRes = allocationInfo.Import(device, handle, *imported, bcInfos.MemProps)))
		{
			device->DestroyBuffer(handle, 0);
			return "Error while importing buffer memory.";
		}
	}
	else
	{
		if (bcInfos.MemoryTypeIndex != UINT32_MAX && info.Temporary && info.Size < vk::Device::TEMP_MEMORY_POOL_BLOCK_SIZE)
		{
			auto it = device->TempMemoryPools.find(bcInfos.MemoryTypeIndex);
			if (it != device->TempMemoryPools.end())
				bcInfos.AllocCreateInfo.pool = it->second;
		}
		if (NOS_VULKAN_FAILED(*outVkRes = vmaCreateBufferWithAlignment(device->Allocator, &bcInfos.BufCreateInfo, &bcInfos.AllocCreateInfo, info.MemProps.Alignment, &handle, &allocationInfo.Handle, &allocationInfo.Info)))
		{
			if (handle)
				device->DestroyBuffer(handle, 0);
			return "Error while creating buffer.";
		}
	}

#ifndef NDEBUG
	VkMemoryRequirements memReq = {};
	device->GetBufferMemoryRequirements(handle, &memReq);
	assert(memReq.size == allocationInfo.GetSize());
#endif

	if (bcInfos.ExtMemHandleType || info.Imported)
		if (NOS_VULKAN_FAILED(*outVkRes = allocationInfo.SetExternalMemoryHandleType(device, info.ExternalMemoryHandleType)))
		{
			assert(!info.Imported);
			device->DestroyBuffer(handle, 0);
			return "Error while setting external memory handle type.";
		}

	return FromExisting(device, handle, info.Usage, info.MemProps.Alignment, info.ElementType, std::move(allocationInfo), info.Size);
}

Buffer::Buffer(Device* device, VkBuffer buffer, VkBufferUsageFlags usage, uint32_t alignment, int elementType, std::optional<Allocation> alloc, VkDeviceSize size)
	: ResourceBase(device), Usage(usage), Alignment(alignment), ElementType(elementType),
	State{ .StageMask = VK_PIPELINE_STAGE_2_NONE,
		  .AccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT }
{
	Handle = buffer;
	Size = size;
	AllocationInfo = std::move(alloc);
}

rc<Buffer> Buffer::FromExisting(Device* device, VkBuffer buffer, VkBufferUsageFlags usage, uint32_t alignment, int elementType, std::optional<Allocation> alloc, VkDeviceSize size)
{
	return New(device, buffer, usage, alignment, elementType, std::move(alloc), size);
}

Result<BufferCreateRequest> Buffer::TryGetRelaxedSuitableCreateRequest(Device* Vk, BufferCreateRequest const& info)
{
	return info;
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