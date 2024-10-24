/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Common.h"
#include "nosVulkan/Platform.h"

// External
#include <vk_mem_alloc.h>

// std
#include <variant>

namespace nos::vk
{

struct nosVulkan_API Allocation
{
	VmaAllocation Handle = 0;
	VmaAllocationInfo Info = {};
	MemoryProperties MemProps;
	NOS_HANDLE OsHandle = 0;
	uint32_t ExternalMemoryHandleType;
	struct ImportInfo
	{
		VkDeviceSize AllocationSize = 0;
		uint64_t PID = 0;
	};
	std::optional<ImportInfo> Imported = std::nullopt;
	void*& Mapping() { return Info.pMappedData; }
	VkDeviceSize GetOffset() const;
	VkDeviceSize GetSize() const;
	VkDeviceSize GetAllocationSize() const;
	VkDeviceMemory GetMemory() const;
	uint32_t GetMemoryTypeIndex() const;
	VkResult Import(Device* device, std::variant<VkBuffer, VkImage> handle, 
		vk::MemoryExportInfo const& imported, VkMemoryPropertyFlags memProps);
	VkResult SetExternalMemoryHandleType(Device* device, uint32_t handleType);
};

template <typename T>
struct nosVulkan_API ResourceBase : DeviceChild
{
	T Handle;
	VkDeviceSize Size;
	std::optional<Allocation> AllocationInfo = std::nullopt;
	using DeviceChild::DeviceChild;
	
	MemoryExportInfo GetExportInfo() const
	{
		if (!AllocationInfo)
			return {};
		return MemoryExportInfo{
			.HandleType = AllocationInfo->ExternalMemoryHandleType,
			.PID    = AllocationInfo->Imported ? AllocationInfo->Imported->PID : PlatformGetCurrentProcessId(),
			.Handle = AllocationInfo->OsHandle,
			.Offset = AllocationInfo->GetOffset(),
			.Size = Size,
			.AllocationSize = AllocationInfo->GetAllocationSize(),
			.MemProps = AllocationInfo->MemProps,
		};
	}
};

}
