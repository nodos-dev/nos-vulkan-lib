// Copyright MediaZ AS. All Rights Reserved.

#include "mzVulkan/Command.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/Image.h"

namespace mz::vk
{

VkResult Queue::Submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
    std::unique_lock lock(Mutex);
    return VklQueueFunctions::Submit(submitCount, pSubmits, fence);
}

VkResult Queue::Submit(std::vector<rc<CommandBuffer>> const& cmd)
{
   std::vector<VkSubmitInfo> infos(cmd.size());
   return Submit(infos.size(), infos.data(), 0);
}

Queue::Queue(Device* Device, u32 Family, u32 Index)
    : VklQueueFunctions{Device}, Family(Family), Idx(Index)
{
    Device->GetDeviceQueue(Family, Index, &handle);
}

Device* Queue::GetDevice()
{
    return static_cast<Device*>(fnptrs);
}

CommandBuffer::CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle)
    : VklCommandFunctions{Pool->GetDevice(), Handle}, Pool(Pool)
{

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    
    MZ_VULKAN_ASSERT_SUCCESS(GetDevice()->CreateFence(&fenceInfo, 0, &Fence));
}

void CommandBuffer::Wait()
{
    MZ_VULKAN_ASSERT_SUCCESS(GetDevice()->WaitForFences(1, &Fence, 0, -1));

    for (auto& fn : Callbacks)
    {
        fn();
    }

    Callbacks.clear();
    WaitGroup.clear();
    SignalGroup.clear();
}

CommandBuffer::~CommandBuffer()
{
    for (auto& fn : Callbacks)
    {
        fn();
    }

    Callbacks.clear();
    WaitGroup.clear();
    SignalGroup.clear();
    
    GetDevice()->DestroyFence(Fence, 0);
    MZ_VULKAN_ASSERT_SUCCESS(Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
}

rc<CommandBuffer> CommandBuffer::Submit()
{
    auto self = shared_from_this();
    for(auto& f : PreSubmit)
    {
        f(self);
    }
    std::vector<VkSemaphore> Signal;
    std::vector<VkSemaphore> Wait;
    std::vector<VkPipelineStageFlags> Stages;
    std::vector<uint64_t> WaitValues;
    std::vector<uint64_t> SignalValues;


    Wait.reserve(WaitGroup.size());
    WaitValues.reserve(WaitGroup.size());
    Stages.reserve(WaitGroup.size());
    Signal.reserve(Signal.size());
    SignalValues.reserve(SignalGroup.size());
    
    for (auto [sema, val] : SignalGroup)
    {
        Signal.push_back(sema);
        SignalValues.push_back(val);
    }

    for (auto [sema, p] : WaitGroup)
    {
        Wait.push_back(sema);
        WaitValues.push_back(p.first);
        Stages.push_back(p.second);
    }

    VkTimelineSemaphoreSubmitInfo timelineInfo = {
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .waitSemaphoreValueCount   = (u32)WaitValues.size(),
      .pWaitSemaphoreValues      = WaitValues.data(),
      .signalSemaphoreValueCount = (u32)SignalValues.size(),
      .pSignalSemaphoreValues    = SignalValues.data(),
    };
    
    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = &timelineInfo,
        .waitSemaphoreCount   = (u32)Wait.size(),
        .pWaitSemaphores      = Wait.data(),
        .pWaitDstStageMask    = Stages.data(),
        .commandBufferCount   = 1,
        .pCommandBuffers      = &handle,
        .signalSemaphoreCount = (u32)Signal.size(),
        .pSignalSemaphores    = Signal.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(End());
    MZ_VULKAN_ASSERT_SUCCESS(Pool->Submit(1, &submitInfo, Fence));

    return self;
}

bool CommandBuffer::Ready()
{
    return VK_SUCCESS == GetDevice()->GetFenceStatus(Fence);
}

Device* CommandBuffer::GetDevice()
{
    return static_cast<Device*>(fnptrs);
}

CommandPool::CommandPool(Device* Vk, rc<vk::Queue> Queue, u64 PoolSize)
    : Queue(Queue), NextBuffer(PoolSize)
{
    VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = Queue->Idx,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateCommandPool(&info, 0, &Handle));

    std::vector<VkCommandBuffer> buf(PoolSize);
 
    VkCommandBufferAllocateInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = Handle,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = (u32)PoolSize,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->AllocateCommandBuffers(&cmdInfo, buf.data()));

    Buffers.reserve(PoolSize);

    for (VkCommandBuffer cmd : buf)
    {
        Buffers.emplace_back(CommandBuffer::New(this, cmd));
    }
}

CommandPool::CommandPool(Device* Vk)
    : CommandPool(Vk, Vk->Queue)
{
}

Device* CommandPool::GetDevice()
{
    return Queue->GetDevice();
}

CommandPool::~CommandPool()
{
    Buffers.clear();
    GetDevice()->DestroyCommandPool(Handle, 0);
}

rc<CommandBuffer> CommandPool::AllocCommandBuffer(VkCommandBufferLevel level)
{
    while (!Buffers[NextBuffer]->Ready())
        NextBuffer++;
    ;

    auto cmd = Buffers[NextBuffer];

    cmd->Wait();

    MZ_VULKAN_ASSERT_SUCCESS(GetDevice()->ResetFences(1, &cmd->Fence));
    MZ_VULKAN_ASSERT_SUCCESS(cmd->Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
    return cmd;
}

rc<CommandBuffer> CommandPool::BeginCmd(VkCommandBufferLevel level)
{
    rc<CommandBuffer> Cmd = AllocCommandBuffer(level);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Cmd->Begin(&beginInfo));

    return Cmd;
}

} // namespace mz::vk