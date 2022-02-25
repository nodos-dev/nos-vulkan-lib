#pragma once

#include "Device.h"
#include "mzVkCommon.h"

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
        return ++val %= max;
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

struct CommandBuffer : SharedFactory<CommandBuffer>,
                       VklCommandFunctions
{
    struct CommandPool* Pool;

    VkFence Fence;

    bool Ready()
    {
        return VK_SUCCESS == GetDevice()->GetFenceStatus(Fence);
    }

    void Wait()
    {
        MZ_VULKAN_ASSERT_SUCCESS(GetDevice()->WaitForFences(1, &Fence, 0, -1));
        // MZ_VULKAN_ASSERT_SUCCESS(GetDevice()->DeviceWaitIdle());
    }

    CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle);

    ~CommandBuffer();

    VulkanDevice* GetDevice()
    {
        return static_cast<VulkanDevice*>(fnptrs);
    }

    void Submit(
        uint32_t                    waitSemaphoreCount,
        const VkSemaphore*          pWaitSemaphores,
        const VkPipelineStageFlags* pWaitDstStageMask,
        uint32_t                    signalSemaphoreCount,
        const VkSemaphore*          pSignalSemaphores);

    void Submit(struct VulkanImage*, VkPipelineStageFlags);
    void Submit(std::vector<struct VulkanImage*>, VkPipelineStageFlags);
};

struct CommandPool : SharedFactory<CommandPool>
{
    static constexpr u64 DefaultPoolSize = 1024;

    VkCommandPool Handle;

    VulkanQueue Queue;

    std::vector<std::shared_ptr<CommandBuffer>> Buffers;
    CircularIndex                               NextBuffer;

    CommandPool(VulkanDevice* Vk, u32 family)
        : Queue(Vk, family, 0), NextBuffer(DefaultPoolSize)
    {
        VkCommandPoolCreateInfo info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = family,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateCommandPool(&info, 0, &Handle));

        VkCommandBuffer buf[DefaultPoolSize];

        VkCommandBufferAllocateInfo cmdInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = Handle,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = DefaultPoolSize,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateCommandBuffers(&cmdInfo, buf));

        Buffers.reserve(DefaultPoolSize);

        for (VkCommandBuffer cmd : buf)
        {
            Buffers.emplace_back(CommandBuffer::New(this, cmd));
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
        while (!Buffers[NextBuffer]->Ready())
            NextBuffer++;
        ;

        auto cmd = Buffers[NextBuffer];

        assert(cmd->Ready());

        GetDevice()->ResetFences(1, &cmd->Fence);
        return cmd;
    }

    std::shared_ptr<CommandBuffer> BeginCmd(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    {
        std::shared_ptr<CommandBuffer> Cmd = AllocCommandBuffer(level);

        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Cmd->Begin(&beginInfo));

        return Cmd;
    }
};

} // namespace mz