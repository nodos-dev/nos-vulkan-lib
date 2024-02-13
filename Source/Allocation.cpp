// Copyright MediaZ AS. All Rights Reserved.

#include "nosVulkan/Allocation.h"

#include "nosVulkan/Device.h"

// External
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace nos::vk
{
VkDeviceSize Allocation::GetOffset() const
{
	return Info.offset;
}

VkDeviceSize Allocation::GetSize() const
{
	return Info.size; 
}

VkDeviceSize Allocation::GetAllocationSize() const 
{
	if (!Handle)
	{
		assert(Imported && "GetAllocationSize called on an non-existent allocation!");
		return Imported->AllocationSize;
	}
	switch (Handle->GetType())
	{
	case VmaAllocation_T::ALLOCATION_TYPE_BLOCK: return Handle->GetBlock()->m_pMetadata->GetSize();
	case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED: return GetSize();
	default: return 0;
	}
}

VkDeviceMemory Allocation::GetMemory() const
{
	return Info.deviceMemory;
}
uint32_t Allocation::GetMemoryTypeIndex() const
{
	return Info.memoryType;
}

VkResult Allocation::Import(Device* device, std::variant<VkBuffer, VkImage> handle, vk::MemoryExportInfo const& imported, VkMemoryPropertyFlags memProps)
{
	auto dupHandle = PlatformDupeHandle(imported.PID, imported.Handle);

	VkMemoryRequirements requirements;
	if (auto buf = std::get_if<VkBuffer>(&handle))
		device->GetBufferMemoryRequirements(*buf, &requirements);
	else
		device->GetImageMemoryRequirements(std::get<VkImage>(handle), &requirements);
	
	// Use GetMemoryWin32HandlePropertiesKHR for memory type bits!
	// TODO: Cannot use this for opaque handle types!
	VkMemoryWin32HandlePropertiesKHR extHandleProps{
		.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR
	};
	auto res = device->GetMemoryWin32HandlePropertiesKHR((VkExternalMemoryHandleTypeFlagBits)imported.HandleType, dupHandle, &extHandleProps);
	NOSVK_ASSERT(res)
	auto [typeIndex, memType] = MemoryTypeIndex(device->PhysicalDevice, extHandleProps.memoryTypeBits, memProps);

	VkImportMemoryWin32HandleInfoKHR importInfo = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
		.handleType = VkExternalMemoryHandleTypeFlagBits(imported.HandleType),
		.handle = dupHandle,
	};

	VkMemoryAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &importInfo,
		.allocationSize = imported.AllocationSize,
		.memoryTypeIndex = typeIndex,
	};

	VkDeviceMemory mem;
	res = device->AllocateMemory(&info, 0, &mem);
	if (NOS_VULKAN_FAILED(res))
		return res;
	Info = VmaAllocationInfo{
		.memoryType = typeIndex,
		.deviceMemory = mem,
		.offset = imported.Offset,
		.size = requirements.size,
	};
	Imported = {.AllocationSize = imported.AllocationSize};
	if (auto buf = std::get_if<VkBuffer>(&handle))
		res = device->BindBufferMemory(*buf, mem, imported.Offset);
	else
		res = device->BindImageMemory(std::get<VkImage>(handle), mem, imported.Offset);
	return res;
}

VkResult Allocation::SetExternalMemoryHandleType(Device* device, uint32_t handleType)
{
	ExternalMemoryHandleType = handleType;
	if (auto type = PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE & handleType)
	{
		std::unique_lock lock(device->MemoryBlocksMutex);
		auto& [handle, ref] = device->MemoryBlocks[Info.deviceMemory];
		if(!handle)
		{
#if _WIN32
			VkMemoryGetWin32HandleInfoKHR getHandleInfo = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
				.memory = GetMemory(),
				.handleType = VkExternalMemoryHandleTypeFlagBits(type),
			};
			auto ret = device->GetMemoryWin32HandleKHR(&getHandleInfo, &handle);
#else
#pragma error "Unimplemented"
#endif
			if (VK_SUCCESS != ret)
				return ret;
		}
		OsHandle = handle;
		++ref;
	}
	return VK_SUCCESS;
}

void Allocation::Release(Device* vk)
{
	std::unique_lock lock(vk->MemoryBlocksMutex);
	auto& [handle, ref] = vk->MemoryBlocks[Info.deviceMemory];
	if (!--ref)
	{
		PlatformCloseHandle(handle);
		vk->MemoryBlocks.erase(Info.deviceMemory);
	}
}

}