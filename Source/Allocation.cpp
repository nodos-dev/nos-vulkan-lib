// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

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
		if (!Imported)
			return 0;
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
	OsHandle = imported.Handle;
	auto dupHandle = PlatformDupeHandle(imported.PID, imported.Handle);
	if (!dupHandle)
		return VK_ERROR_INVALID_EXTERNAL_HANDLE;

	VkMemoryRequirements requirements;
	if (auto buf = std::get_if<VkBuffer>(&handle))
		device->GetBufferMemoryRequirements(*buf, &requirements);
	else
		device->GetImageMemoryRequirements(std::get<VkImage>(handle), &requirements);

	VkResult res = VK_SUCCESS;

	// TODO: Get memory type bits from MemoryExportInfo.
	auto memoryTypeBits = requirements.memoryTypeBits;
	auto handleType = VkExternalMemoryHandleTypeFlagBits(imported.HandleType);
	if (handleType > VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT)
	{
#if _WIN32 // TODO: Other platforms.
		VkMemoryWin32HandlePropertiesKHR extHandleProps{
		.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR
		};
		res = device->GetMemoryWin32HandlePropertiesKHR(VkExternalMemoryHandleTypeFlagBits(imported.HandleType), dupHandle, &extHandleProps);
#else
#pragma error "Unimplemented"
#endif
		if (NOS_VULKAN_FAILED(res))
			return res;
		memoryTypeBits = extHandleProps.memoryTypeBits;
	}
	auto [typeIndex, memType] = MemoryTypeIndex(device->PhysicalDevice, memoryTypeBits, memProps);

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
	Imported = {.AllocationSize = imported.AllocationSize, .PID = imported.PID};
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
		auto& handle= device->MemoryBlocks[Info.deviceMemory];
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
	}
	return VK_SUCCESS;
}

}