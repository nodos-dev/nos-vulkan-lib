#pragma once

#include "vulkan/vulkan_core.h"
#include <mzVkCommon.h>

namespace mz::vk
{

struct Device;

struct mzVulkan_API Queue : SharedFactory<Queue>, VklQueueFunctions
{
    u32 Family;
    u32 Idx;
    std::mutex Mutex;

    Queue(Device* Device, u32 Family, u32 Index);
    Device* GetDevice();
    VkResult Submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
};

template <class T>
void EndResourceDependency(rc<T> Resource)
{
    if constexpr (std::same_as<T, Image>)
    {
        Resource->State.AccessMask = VK_ACCESS_NONE;
        Resource->State.StageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

struct mzVulkan_API CommandBuffer : SharedFactory<CommandBuffer>,
                                    VklCommandFunctions
{
    CommandPool* Pool;

    VkFence Fence;

    std::vector<std::function<void()>> Callbacks;
    std::map<VkSemaphore, VkPipelineStageFlags> WaitGroup;
    std::set<VkSemaphore> SignalGroup;

    bool Ready();
    void Wait();

    CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle);

    ~CommandBuffer();

    Device* GetDevice();
    rc<CommandBuffer> Submit();

    template <class... T>
    void AddDependency(rc<T>... Resources)
    {
        Callbacks.push_back([Resources...]() {
            (EndResourceDependency(Resources), ...);
        });
    }
};

struct mzVulkan_API CommandPool : SharedFactory<CommandPool>
{
    static constexpr u64 DefaultPoolSize = 64;

    VkCommandPool Handle;

    rc<Queue> Queue;

    std::vector<rc<CommandBuffer>> Buffers;
    CircularIndex NextBuffer;

    CommandPool(Device* Vk);

    Device* GetDevice();

    ~CommandPool();

    rc<CommandBuffer> AllocCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    rc<CommandBuffer> BeginCmd(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkResult Submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
    {
        return Queue->Submit(submitCount, pSubmits, fence);
    }
};

} // namespace mz::vk