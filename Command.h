#pragma once

#include <Device.h>

namespace mz::vk
{

struct mzVulkan_API Queue : VklQueueFunctions
{
    u32 Family;
    u32 Index;

    Queue(Device* Device, u32 Family, u32 Index);
    Device* GetDevice();
};

struct mzVulkan_API CommandBuffer : SharedFactory<CommandBuffer>,
                                    VklCommandFunctions
{
    struct CommandPool* Pool;

    VkFence Fence;

    std::vector<std::function<void()>> Callbacks;

    bool Ready();

    void Wait();

    CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle);

    ~CommandBuffer();

    Device* GetDevice();
    void Submit(View<VkSemaphore> Wait = {}, View<VkPipelineStageFlags> Stages = {}, View<VkSemaphore> Signal = {});
    void Submit(std::shared_ptr<struct Image>, VkPipelineStageFlags);
    void Submit(View<std::shared_ptr<struct Image>>, VkPipelineStageFlags);
};

struct mzVulkan_API CommandPool : SharedFactory<CommandPool>
{
    static constexpr u64 DefaultPoolSize = 1024;

    VkCommandPool Handle;

    Queue Queue;

    std::vector<std::shared_ptr<CommandBuffer>> Buffers;
    CircularIndex NextBuffer;

    CommandPool(Device* Vk, u32 family);

    Device* GetDevice();

    ~CommandPool();

    std::shared_ptr<CommandBuffer> AllocCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    std::shared_ptr<CommandBuffer> BeginCmd(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
};

} // namespace mz::vk