#pragma once

#include "Common.h"

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
	void* OsHandle = 0;
	VkDeviceSize Size = 0;
	uint32_t ExternalMemoryHandleType;
	bool Imported = false;
	void*& Mapping() { return Info.pMappedData; }
	VkDeviceSize GetOffset() const;
	VkDeviceSize GetSize() const;
	VkDeviceSize GetAllocationSize() const;
	VkDeviceMemory GetMemory() const;
	uint32_t GetMemoryTypeIndex() const;
	VkResult Import(Device* device, std::variant<VkBuffer, VkImage> handle, 
		vk::MemoryExportInfo const& imported, VkMemoryPropertyFlags memProps);
	VkResult SetExternalMemoryHandleType(Device* device, uint32_t handleType);

	void Release(Device* vk);
};

template <typename T>
struct nosVulkan_API ResourceBase : DeviceChild
{
	T Handle;
	Allocation Allocation;
	using DeviceChild::DeviceChild;
	
	MemoryExportInfo GetExportInfo() const
	{
		return MemoryExportInfo{
			.HandleType = Allocation.ExternalMemoryHandleType,
			.PID    = PlatformGetCurrentProcessId(),
			.Handle = Allocation.OsHandle,
			.Offset = Allocation.GetOffset(),
			.AllocationSize = Allocation.GetAllocationSize()
		};
	}
	
	~ResourceBase() override
	{
		Allocation.Release(GetDevice());
	}
};

}
