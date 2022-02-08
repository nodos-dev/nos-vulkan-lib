#pragma once

#define VK_USE_PLATFORM_WIN32_KHR

#include "Common.h"

struct Allocator
{
    VkInstance       instance;
    VkPhysicalDevice pdevice;
    VkDevice         device;

    VkDeviceMemory AllocateMemory(VkMemoryDedicatedAllocateInfo const& dedicatedAllocateInfo, u64 size, u32 memType)
    {
        VkExportMemoryWin32HandleInfoKHR handleInfo = {
            .sType    = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
            .pNext    = &dedicatedAllocateInfo,
            .dwAccess = GENERIC_ALL,
        };

        VkExportMemoryAllocateInfo exportInfo = {
            .sType       = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
            .pNext       = &handleInfo,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
        };

        VkMemoryAllocateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = &exportInfo,
            .allocationSize  = size,
            .memoryTypeIndex = memType,
        };

        VkDeviceMemory mem;
        CHECKRE(vkAllocateMemory(device, &info, 0, &mem));

        return mem;
    }

    VkDeviceMemory AllocateBufferMemory(VkBuffer buffer, u64* size)
    {
        VkMemoryRequirements             req;
        VkPhysicalDeviceMemoryProperties props;

        vkGetBufferMemoryRequirements(device, buffer, &req);
        vkGetPhysicalDeviceMemoryProperties(pdevice, &props);
        *size = req.size;
        return AllocateMemory(VkMemoryDedicatedAllocateInfo{
                                  .sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                                  .buffer = buffer,
                              },
                              req.size,
                              log2(req.memoryTypeBits - (req.memoryTypeBits & (req.memoryTypeBits - 1))));
    }

    VkDeviceMemory AllocateImageMemory(VkImage image)
    {
        VkMemoryRequirements             req;
        VkPhysicalDeviceMemoryProperties props;

        vkGetImageMemoryRequirements(device, image, &req);
        vkGetPhysicalDeviceMemoryProperties(pdevice, &props);

        u32 memType = log2(req.memoryTypeBits - (req.memoryTypeBits & (req.memoryTypeBits - 1)));

        return AllocateMemory(VkMemoryDedicatedAllocateInfo{
                                  .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                                  .image = image,
                              },
                              req.size,
                              log2(req.memoryTypeBits - (req.memoryTypeBits & (req.memoryTypeBits - 1))));
    }
};
