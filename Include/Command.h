#pragma once

#include <mzVkCommon.h>

namespace mz::vk
{

struct Device;

struct mzVulkan_API Queue : VklQueueFunctions
{
    u32 Family;
    u32 Idx;

    Queue(Device* Device, u32 Family, u32 Index);
    Device* GetDevice();
};

struct mzVulkan_API CommandBuffer : SharedFactory<CommandBuffer>,
                                    VklCommandFunctions
{

    struct CommandPool* Pool;

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

    rc<CommandBuffer> Enqueue(rc<struct Image>, VkPipelineStageFlags);
    rc<CommandBuffer> Enqueue(View<rc<struct Image>>, VkPipelineStageFlags);

    template <class... T>
    void AddDependency(rc<T>... Resources)
    {
        Callbacks.push_back([Resources...]() {});
    }
};

struct mzVulkan_API CommandPool : SharedFactory<CommandPool>
{
    static constexpr u64 DefaultPoolSize = 64;

    VkCommandPool Handle;

    Queue Queue;

    std::vector<rc<CommandBuffer>> Buffers;
    CircularIndex NextBuffer;

    CommandPool(Device* Vk, u32 family);

    Device* GetDevice();

    ~CommandPool();

    rc<CommandBuffer> AllocCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    rc<CommandBuffer> BeginCmd(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
};

} // namespace mz::vk