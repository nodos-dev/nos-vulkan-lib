
#pragma once

#include "Common.h"

struct CommandPool
{
    VkDevice      device;
    VkCommandPool handle;
    u32           family;

    CommandPool(VkDevice device, u32 family)
        : device(device), family(family)
    {
        VkCommandPoolCreateInfo info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = family,
        };

        vkCreateCommandPool(device, &info, 0, &handle);
    }

    ~CommandPool()
    {
        vkDestroyCommandPool(device, handle, 0);
    }

    std::optional<VkCommandBuffer> AllocCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        VkCommandBuffer cmd;

        VkCommandBufferAllocateInfo cmd_info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = handle,
            .level              = level,
            .commandBufferCount = 1,
        };

        if (VkResult re = vkAllocateCommandBuffers(device, &cmd_info, &cmd))
        {
            return {};
        }

        return cmd;
    }

    std::optional<std::vector<VkCommandBuffer>> AllocCommandBuffers(u32 count, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        std::vector<VkCommandBuffer> buf(count);

        VkCommandBufferAllocateInfo cmd_info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = handle,
            .level              = level,
            .commandBufferCount = count,
        };

        if (VkResult re = vkAllocateCommandBuffers(device, &cmd_info, buf.data()))
        {
            return {};
        }

        return buf;
    }

    VkResult Reset()
    {
        return vkResetCommandPool(device, handle, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    }

    template <class F>
    void Exec(VkQueue queue, F&& f)
    {
        if (auto mbyCmd = AllocCommandBuffer())
        {
            VkCommandBuffer cmd = mbyCmd.value();

            VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            VkFence           fence;

            CHECKRE(vkCreateFence(device, &fenceInfo, 0, &fence));

            VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };

            CHECKRE(vkBeginCommandBuffer(cmd, &begin_info));

            f(cmd);

            CHECKRE(vkEndCommandBuffer(cmd));

            VkSubmitInfo submit_info = {
                .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers    = &cmd,
            };

            CHECKRE(vkQueueSubmit(queue, 1, &submit_info, fence));
            CHECKRE(vkWaitForFences(device, 1, &fence, 1, -1));
            vkDestroyFence(device, fence, 0);
        }
    }
};
