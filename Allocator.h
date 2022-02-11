#pragma once

#include <map>
#include <variant>
#include <assert.h>

#define VK_USE_PLATFORM_WIN32_KHR

#include "Common.h"

struct Block
{
    VkDeviceMemory        memory;
    VkMemoryPropertyFlags memProps;
    u32                   typeIndex;
    u64                   allocationSize;
};

struct Allocator
{
    VkInstance       instance;
    VkPhysicalDevice pdevice;
    VkDevice         device;

    u32 MemoryTypeIndex(u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps)
    {
        if (0 == requestedProps)
        {
            return log2(memoryTypeBits - (memoryTypeBits & (memoryTypeBits - 1)));
        }

        VkPhysicalDeviceMemoryProperties props;
        vkGetPhysicalDeviceMemoryProperties(pdevice, &props);

        std::vector<std::pair<u32, u32>> memoryTypes;

        for (int i = 0; i < props.memoryTypeCount; i++)
        {
            if (memoryTypeBits & (1 << i))
            {
                memoryTypes.push_back(std::make_pair(i, std::popcount(props.memoryTypes[i].propertyFlags & requestedProps)));
            }
        }

        std::sort(memoryTypes.begin(), memoryTypes.end(), [](const std::pair<u32, u32>& a, const std::pair<u32, u32>& b) { return a.second > b.second; });

        return memoryTypes.front().first;
    }

    VkDeviceMemory AllocateResourceMemory(std::variant<VkBuffer, VkImage> resource, HANDLE* handle)
    {
        VkMemoryRequirements             req;
        VkPhysicalDeviceMemoryProperties props;

        VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        };

        if (std::holds_alternative<VkBuffer>(resource))
        {
            dedicatedAllocateInfo.buffer = std::get<VkBuffer>(resource);
            vkGetBufferMemoryRequirements(device, std::get<VkBuffer>(resource), &req);
        }

        else if (std::holds_alternative<VkImage>(resource))
        {
            dedicatedAllocateInfo.image = std::get<VkImage>(resource);
            vkGetImageMemoryRequirements(device, std::get<VkImage>(resource), &req);
        }
        else
        {
            assert(false && "Unreachable path");
        }

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
            .allocationSize  = req.size,
            .memoryTypeIndex = MemoryTypeIndex(req.memoryTypeBits, 0),
        };

        VkDeviceMemory mem;
        CHECKRE(vkAllocateMemory(device, &info, 0, &mem));

        if (handle)
        {
            VkMemoryGetWin32HandleInfoKHR handleInfo = {
                .sType      = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                .memory     = mem,
                .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            };

            CHECKRE(vkGetMemoryWin32HandleKHR(device, &handleInfo, handle));
        }

        return mem;
    }
};
