
#pragma once

#include "Device.h"
#include "mzCommon.h"
#include "vulkan/vulkan_core.h"
#include <memory>
#include <utility>

struct VulkanQueue : VklQueueFunctions
{
    u32 Family;
    u32 Index;

    VulkanQueue(VulkanDevice* Device, u32 Family, u32 Index)
        : VklQueueFunctions{Device}, Family(Family), Index(Index)
    {
        Device->GetDeviceQueue(Family, Index, &handle);
    }

    VulkanDevice* GetDevice()
    {
        return static_cast<VulkanDevice*>(fnptrs);
    }
};

struct CommandBuffer : std::enable_shared_from_this<CommandBuffer>,
                       VklCommandFunctions,
                       Uncopyable
{
    struct CommandPool* Pool;

    CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle);
    ~CommandBuffer();

    VulkanDevice* GetDevice()
    {
        return static_cast<VulkanDevice*>(fnptrs);
    }
};

struct CommandPool : std::enable_shared_from_this<CommandPool>, Uncopyable
{
    VkCommandPool Handle;

    VulkanQueue Queue;

    CommandPool(VulkanDevice* Vk, u32 family)
        : Queue(Vk, family, 0)
    {
        VkCommandPoolCreateInfo info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = family,
        };

        CHECKRE(Vk->CreateCommandPool(&info, 0, &Handle));
    }

    VulkanDevice* GetDevice()
    {
        return Queue.GetDevice();
    }

    ~CommandPool()
    {
        GetDevice()->DestroyCommandPool(Handle, 0);
    }

    std::shared_ptr<CommandBuffer> AllocCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        VkCommandBuffer cmd;

        VkCommandBufferAllocateInfo info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = Handle,
            .level              = level,
            .commandBufferCount = 1,
        };

        CHECKRE(GetDevice()->AllocateCommandBuffers(&info, &cmd));
        return std::make_shared<CommandBuffer>(this, cmd);
    }

    template <class F>
    void Exec(F&& f)
    {
        std::shared_ptr<CommandBuffer> cmd = AllocCommandBuffer();

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        CHECKRE(cmd->Begin(&beginInfo));
        f(cmd);
        CHECKRE(cmd->End());

        VkSubmitInfo submitInfo = {
            .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers    = &cmd->handle,
        };

        CHECKRE(Queue.Submit(1, &submitInfo, 0));
        CHECKRE(Queue.WaitIdle());
    }
};
