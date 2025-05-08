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
	return vk::BufferCreateRequest {
		.Resource = {
			.Temporary = true,
			.ExternalMemory = VkExternalMemoryHandleTypeFlags(0),
		},
		.Size = size,
		.Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.MemProps = {.Mapped = true, .VRAM = false, .Download = false,},
	};
}

Result<BufferCreationInfos> CalculateBufferCreationInfos(vk::Device* device, BufferCreateRequest const& request)
{
	BufferCreationInfos ret;
	uint32_t extMemHandleType = 0;
	if (auto importInfo = request.Resource.GetImportInfo())
		extMemHandleType = importInfo->HandleType;
	else if (request.Resource.GetExportHandleTypes())
		extMemHandleType = request.Resource.GetExportHandleTypes();
	auto requestedMemProps = request.MemProps;
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
		.size = request.Size,
		.usage = request.Usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if(requestedMemProps.VRAM && requestedMemProps.ForceHostMemory)
		return "Buffer requested to be created in VRAM but also forced to be created in host memory.";

	auto& memProps = (ret.MemProps = 0);
	if (requestedMemProps.VRAM)
		memProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (requestedMemProps.Mapped)
		memProps |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	if (request.Resource.IsImported())
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


Result<rc<Buffer>> Buffer::Create(Device* device, BufferCreateRequest const& request, VkResult* outVkRes)
{
	VkResult res{};
	if (!outVkRes)
		outVkRes = &res;
	// Might return error without vulkan failure
	*outVkRes = VK_SUCCESS;

	auto allocationInfo = vk::Allocation{};
	auto bcInfosRes = CalculateBufferCreationInfos(device, request);
	if (auto* err = bcInfosRes.Error())
		return std::move(*err);
	
	auto& bcInfos = *bcInfosRes.Get();
	allocationInfo.MemProps = request.MemProps;

	VkBuffer handle{};
	if (auto* imported = request.Resource.GetImportInfo())
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
		if (bcInfos.MemoryTypeIndex != UINT32_MAX && request.Resource.Temporary && request.Size < vk::Device::TEMP_MEMORY_POOL_BLOCK_SIZE)
		{
			auto it = device->TempMemoryPools.find(bcInfos.MemoryTypeIndex);
			if (it != device->TempMemoryPools.end())
				bcInfos.AllocCreateInfo.pool = it->second;
		}
		if (NOS_VULKAN_FAILED(*outVkRes = vmaCreateBufferWithAlignment(device->Allocator, &bcInfos.BufCreateInfo, &bcInfos.AllocCreateInfo, request.MemProps.Alignment, &handle, &allocationInfo.Handle, &allocationInfo.Info)))
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

	if (bcInfos.ExtMemHandleType)
		if (NOS_VULKAN_FAILED(*outVkRes = allocationInfo.SetExternalMemoryHandleType(device, bcInfos.ExtMemHandleType)))
		{
			assert(!request.Resource.IsImported());
			device->DestroyBuffer(handle, 0);
			return "Error while setting external memory handle type.";
		}

	return FromExisting(device, handle, request.Usage, request.MemProps.Alignment, request.ElementType, std::move(allocationInfo), request.Size);
}

Buffer::Buffer(Device* device, VkBuffer buffer, VkBufferUsageFlags usage, uint32_t alignment, int elementType, std::optional<Allocation> alloc, VkDeviceSize size)
	: ResourceBase(device), Usage(usage), Alignment(alignment), ElementType(elementType),
	State{ .StageMask = VK_PIPELINE_STAGE_2_NONE, .AccessMask = VK_ACCESS_2_NONE }
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
	auto request = info;

	if (request.MemProps.VRAM && request.MemProps.ForceHostMemory)
	{
		GLog.W("Buffer requested to be created in VRAM but also forced to be created in host memory");
		request.MemProps.VRAM = false;
	}

	if (auto res = CalculateBufferCreationInfos(Vk, request); auto err = res.Error())
	{
		if (request.Resource.IsImported())
			return *err;
		if (!request.Resource.ShouldExport())
			return *err;
		GLog.W("CreateBuffer: Failed to calculate buffer creation info(%s), trying without exporting memory.", err->c_str());
		request.Resource.ExternalMemory = VkExternalMemoryHandleTypeFlags(0);
		if (auto res = CalculateBufferCreationInfos(Vk, request); auto err = res.Error())
			return *err;
	}
	return request;
}

Result<rc<Buffer>> Buffer::CreateRelaxed(Device* Vk, BufferCreateRequest const& createInfo, VkResult* vkRes)
{
	if (vkRes)
		*vkRes = VK_SUCCESS;
	if (auto res = TryGetRelaxedSuitableCreateRequest(Vk, createInfo); auto val = res.Get())
		return Create(Vk, *val, vkRes);
	else
		return *res.Error();
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

	Src->Transition(Cmd,
					BufferMemoryState{.StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
									  .AccessMask = VK_ACCESS_2_TRANSFER_READ_BIT},
					Region->srcOffset,
					Region->size);
	Transition(Cmd,
			   BufferMemoryState{.StageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
								 .AccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT},
			   Region->dstOffset,
			   Region->size);
	
    Cmd->CopyBuffer(Src->Handle, this->Handle, 1, Region);
}

void Buffer::Transition(rc<CommandBuffer> curCmd, BufferMemoryState dst, VkDeviceSize offset, VkDeviceSize size)
{
	dst.QueueFamilyIndex = curCmd->Pool->PoolQueue->FamilyIndex;
	if (State.PreviousCmd && State.PreviousCmd->Pool->PoolQueue->FamilyIndex != dst.QueueFamilyIndex)
	{
		// Previous command buffer is in a different queue family. Add wait semaphore to current command buffer.
		curCmd->WaitGroup[State.PreviousCmd->FinishedSem->Handle] = {State.PreviousCmd->SubmitCount, 0};
	}
	if (Vk->Features.synchronization2)
	{
		VkBufferMemoryBarrier2 barrier {
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask = State.StageMask,
			.srcAccessMask = State.AccessMask,
			.dstStageMask = dst.StageMask,
			.dstAccessMask = dst.AccessMask,
			.srcQueueFamilyIndex = State.QueueFamilyIndex,
			.dstQueueFamilyIndex = dst.QueueFamilyIndex,
			.buffer = this->Handle,
			.offset = offset,
			.size = size,
		};
		VkDependencyInfo depInfo = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.dependencyFlags = 0,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &barrier,
		};
		curCmd->PipelineBarrier2(&depInfo);
	}
	else
	{
		GLog.E("BufferTransition: Memory barriers are currently only implemented for synchronization2!");
	}
	State = dst;
	State.PreviousCmd = curCmd;
	curCmd->Callbacks.push_back([this, cmd=curCmd.get()] {
		if (State.PreviousCmd.get() == cmd)
			State.PreviousCmd = nullptr;
	});
	curCmd->AddDependency(shared_from_this());
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
		{
			Vk->DestroyBuffer(Handle, 0);
			Vk->OnMemoryFreed(AllocationInfo->GetMemory());
			Vk->FreeMemory(AllocationInfo->GetMemory(), 0);
		}
		else if (AllocationInfo->Handle)
		    vmaDestroyBuffer(Vk->Allocator, Handle, AllocationInfo->Handle);
    }
}

} // namespace nos::vk