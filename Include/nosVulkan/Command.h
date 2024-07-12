/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Common.h"

namespace nos::vk
{

struct Device;

struct nosVulkan_API Queue : SharedFactory<Queue>, VklQueueFunctions
{
    u32 Family;
    u32 Idx;
    std::mutex Mutex;

    Queue(Device* Device, u32 Family, u32 Index);
    Device* GetDevice();
    VkResult Submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence);
    VkResult Submit(std::vector<rc<CommandBuffer>> const& cmd);
    void Wait() 
    {
        std::unique_lock lock(Mutex);
        WaitIdle();
    }
};

template <class T>
void EndResourceDependency(rc<T> Resource)
{
    if constexpr (std::same_as<T, Image>)
    {
        Resource->State.AccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        Resource->State.StageMask  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
}

struct nosVulkan_API CommandBuffer : SharedFactory<CommandBuffer>,
                                    VklCommandFunctions
{
    enum State 
    { 
        Initial,
        Recording,
        Executable,
        Pending,
        Invalid
    };

    CommandPool* Pool;
    VkFence Fence;
    std::vector<std::function<void()>> Callbacks;
    std::vector<std::function<void(rc<CommandBuffer>)>> PreSubmit;
    std::map<VkSemaphore, std::pair<uint64_t, VkPipelineStageFlags>> WaitGroup;
    std::map<VkSemaphore, uint64_t> SignalGroup;
    std::atomic<State> State = Initial;
    bool IsFree();
	bool Wait(uint64_t timeOutNs = 3000000000ull);
	void WaitAndClear();
    void Clear();
    VkResult Begin(const VkCommandBufferBeginInfo* info);
	void UpdatePendingState();
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
protected:
    VkResult End();
};

struct nosVulkan_API CommandPool : SharedFactory<CommandPool>
{
    static constexpr u64 DefaultPoolSize = 256;

    VkCommandPool Handle;
    rc<Queue> Queue;
    std::vector<rc<CommandBuffer>> Buffers;
    CircularIndex<> NextBuffer;

    CommandPool(Device* Vk);
    CommandPool(Device* Vk, rc<vk::Queue> Queue, u64 PoolSize = DefaultPoolSize);

    Device* GetDevice();

    ~CommandPool();

    rc<CommandBuffer> AllocCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    rc<CommandBuffer> BeginCmd(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkResult Submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
    {
        return Queue->Submit(submitCount, pSubmits, fence);
    }
    void Clear();
};

} // namespace nos::vk