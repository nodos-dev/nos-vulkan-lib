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
#if defined(__APPLE__)
	// When set, the image is IOSurface-backed (no VkDeviceMemory to free). Typed
	// as void* to avoid pulling IOSurface.h into public headers. Releasing is
	// handled in ~Image. OsHandle holds the IOSurfaceID.
	void* MetalIOSurface = nullptr;
#endif
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
			.AllocationSize = AllocationInfo->GetAllocationSize(),
			.MemProps = AllocationInfo->MemProps,
		};
	}
};

}
