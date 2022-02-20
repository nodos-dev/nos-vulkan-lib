#pragma once

#include "Device.h"

namespace mz
{

struct CircularIndex
{
    u64 val;
    u64 max;

    explicit CircularIndex(u64 max)
        : val(0), max(max)
    {
    }

    u64 operator++()
    {
        val++;
        if (val >= max)
            val = 0;
        return val;
    }

    u64 operator++(int)
    {
        u64 ret = val;
        val++;
        if (val >= max)
            val = 0;
        return ret;
    }

    u64 operator--()
    {
        if (val == 0)
            val = max;
        val--;
        return val;
    }

    u64 operator--(int)
    {
        u64 ret = val;
        if (val == 0)
            val = max;
        val--;
        return ret;
    }

    operator u64()
    {
        return val;
    }
};

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

    VkFence Fence;

    bool Ready()
    {
        return VK_SUCCESS == GetDevice()->GetFenceStatus(Fence);
    }

    CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle);

    ~CommandBuffer();

    VulkanDevice* GetDevice()
    {
        return static_cast<VulkanDevice*>(fnptrs);
    }

    void Submit(
        uint32_t                    waitSemaphoreCount   = 0,
        const VkSemaphore*          pWaitSemaphores      = 0,
        const VkPipelineStageFlags* pWaitDstStageMask    = 0,
        uint32_t                    signalSemaphoreCount = 0,
        const VkSemaphore*          pSignalSemaphores    = 0);

    void Submit(std::shared_ptr<struct VulkanImage>, VkPipelineStageFlags);
};

struct CommandPool : std::enable_shared_from_this<CommandPool>, Uncopyable
{
    VkCommandPool Handle;

    VulkanQueue Queue;

    std::vector<std::shared_ptr<CommandBuffer>> Buffers;
    CircularIndex                               NextBuffer;

    CommandPool(VulkanDevice* Vk, u32 family)
        : Queue(Vk, family, 0), NextBuffer(256)
    {
        VkCommandPoolCreateInfo info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = family,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateCommandPool(&info, 0, &Handle));

        VkCommandBuffer buf[256];

        VkCommandBufferAllocateInfo cmdInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = Handle,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 256,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateCommandBuffers(&cmdInfo, buf));

        Buffers.reserve(256);

        for (VkCommandBuffer cmd : buf)
        {
            Buffers.emplace_back(std::make_shared<CommandBuffer>(this, cmd));
        }
    }

    VulkanDevice* GetDevice()
    {
        return Queue.GetDevice();
    }

    ~CommandPool()
    {
        Buffers.clear();
        GetDevice()->DestroyCommandPool(Handle, 0);
    }

    std::shared_ptr<CommandBuffer> AllocCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        while (!Buffers[++NextBuffer]->Ready())
            ;

        auto cmd = Buffers[NextBuffer];

        GetDevice()->ResetFences(1, &cmd->Fence);
        return cmd;
    }

    // template <class F>
    // void Exec(F&& f, VkPipelineStageFlags* stage = 0, const VkSemaphore* wait = 0, const VkSemaphore* signal = 0)
    // {
    //     std::shared_ptr<CommandBuffer> cmd = AllocCommandBuffer();

    //     VkCommandBufferBeginInfo beginInfo = {
    //         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    //         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    //     };

    //     MZ_VULKAN_ASSERT_SUCCESS(cmd->Begin(&beginInfo));
    //     f(cmd);
    //     MZ_VULKAN_ASSERT_SUCCESS(cmd->End());

    //     VkSubmitInfo submitInfo = {
    //         .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    //         .waitSemaphoreCount   = 0 != wait,
    //         .pWaitSemaphores      = wait,
    //         .pWaitDstStageMask    = stage,
    //         .commandBufferCount   = 1,
    //         .pCommandBuffers      = &cmd->handle,
    //         .signalSemaphoreCount = 0 != signal,
    //         .pSignalSemaphores    = signal,
    //     };

    //     MZ_VULKAN_ASSERT_SUCCESS(Queue.Submit(1, &submitInfo, 0));
    //     MZ_VULKAN_ASSERT_SUCCESS(Queue.WaitIdle());
    // }
};

} // namespace mz