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
	auto memory = PlatformDupeHandle(imported.PID, imported.Memory);

	VkMemoryRequirements requirements;
	if (auto buf = std::get_if<VkBuffer>(&handle))
		device->GetBufferMemoryRequirements(*buf, &requirements);
	else
		device->GetImageMemoryRequirements(std::get<VkImage>(handle), &requirements);
	const u64 size = imported.Offset + requirements.size;
	
	// TODO: Use GetMemoryWin32HandlePropertiesKHR?
	auto [typeIndex, memType] = MemoryTypeIndex(device->PhysicalDevice, requirements.memoryTypeBits, memProps);

	VkImportMemoryWin32HandleInfoKHR importInfo = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
		.handleType = VkExternalMemoryHandleTypeFlagBits(imported.Type),
		.handle = memory,
	};

	VkMemoryAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &importInfo,
		.allocationSize = size,
		.memoryTypeIndex = typeIndex,
	};

	VkDeviceMemory mem;
	VkResult res = device->AllocateMemory(&info, 0, &mem);
	if (NOS_VULKAN_FAILED(res))
		return res;
	Info = VmaAllocationInfo{
		.memoryType = typeIndex,
		.deviceMemory = mem,
		.offset = imported.Offset,
		.size = size,
	};
	Imported = true;
	if (auto buf = std::get_if<VkBuffer>(&handle))
		res = device->BindBufferMemory(*buf, mem, imported.Offset);
	else
		res = device->BindImageMemory(std::get<VkImage>(handle), mem, imported.Offset);
	return res;
}

VkResult Allocation::SetExternalMemoryHandleTypes(Device* device, VkExternalMemoryHandleTypeFlags handleTypes)
{
	ExternalMemoryHandleTypes = handleTypes;
	if (auto type = PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE & handleTypes)
	{
		device->MemoryBlocksMutex.lock();
		auto& [handle, ref] = device->MemoryBlocks[Info.deviceMemory];
		if(!handle)
		{
#if _WIN32
		VkMemoryGetWin32HandleInfoKHR getHandleInfo = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
			.memory = GetMemory(),
			.handleType = VkExternalMemoryHandleTypeFlagBits(type),
		};
		return device->GetMemoryWin32HandleKHR(&getHandleInfo, &handle);
#else
#pragma error "Unimplemented"
#endif
		}
		OsHandle = handle;
		++ref;
		device->MemoryBlocksMutex.unlock();
	}
	return VK_SUCCESS;
}

void Allocation::Release(Device* vk)
{
	vk->MemoryBlocksMutex.lock();
	auto& [handle, ref] = vk->MemoryBlocks[Info.deviceMemory];
	if (!--ref)
	{
		PlatformCloseHandle(handle);
		vk->MemoryBlocks.erase(Info.deviceMemory);
	}
	vk->MemoryBlocksMutex.unlock();
}

}