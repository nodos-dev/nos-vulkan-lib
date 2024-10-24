// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "nosVulkan/Command.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/Image.h"
#include "vkl.h"

namespace nos::vk
{

VkResult Queue::Submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
    std::unique_lock lock(Mutex);
	GetDevice()->SubmitCount += submitCount;
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
    
    NOSVK_ASSERT(GetDevice()->CreateFence(&fenceInfo, 0, &Fence));
	Clear();
}

bool CommandBuffer::Wait(uint64_t timeOutNs)
{
	if (GetDevice()->WaitForFences(1, &Fence, 0, timeOutNs) != VK_SUCCESS)
    {
		GLog.W("Command buffer wait timeout!");
        return false;
    }
	Clear();
	return true;
}

void CommandBuffer::WaitAndClear()
{
	if (State == Pending && GetDevice()->WaitForFences(1, &Fence, 0, UINT64_MAX) != VK_SUCCESS)
        GLog.E("Clearing command buffer without finishing: Thread %d", std::this_thread::get_id());
	Clear();
}

void CommandBuffer::Clear()
{
	NOSVK_ASSERT(GetDevice()->ResetFences(1, &Fence));
	NOSVK_ASSERT(VklCommandFunctions::Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
    for (auto& fn : Callbacks) fn();
    Callbacks.clear();
    WaitGroup.clear();
    SignalGroup.clear();
	State = Initial;
}

VkResult CommandBuffer::Begin(const VkCommandBufferBeginInfo* info)
{
    assert(SignalGroup.empty());
    assert(WaitGroup.empty());
    if(!Pool || Initial != State)
        return VK_INCOMPLETE;
    VkResult re = VklCommandFunctions::Begin(info);
    State = Recording;
    return re;
}

VkResult CommandBuffer::End()
{
    if (!Pool || Recording != State)
        return VK_INCOMPLETE;
    VkResult re = VklCommandFunctions::End();
    State = Executable;
    return re;
}

void CommandBuffer::UpdatePendingState()
{
	if (State != Pending)
		return;
	if (GetDevice()->GetFenceStatus(Fence) != VK_SUCCESS)
		return;
	Clear();
}

CommandBuffer::~CommandBuffer()
{
    WaitAndClear();
    GetDevice()->DestroyFence(Fence, 0);
}

rc<CommandBuffer> CommandBuffer::Submit()
{
    auto self = shared_from_this();
    for(auto& f : PreSubmit)
    {
        f(self);
    }
    
    if (!Pool)
		return 0;

    if (Recording == State)
    {
        NOSVK_ASSERT(End());

    }
    if (Executable != State)
        return 0;
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

	auto res = Pool->Submit(1, &submitInfo, Fence);
    NOSVK_ASSERT(res);
    State = Pending;
    return self;
}

bool CommandBuffer::IsFree()
{
	if (State == Initial)
		return true;
	if (State != Pending)
		return false;
    if (GetDevice()->GetFenceStatus(Fence) == VK_SUCCESS)
    {
		Clear();
		return true;
    }
	return false;
}

Device* CommandBuffer::GetDevice()
{
    return static_cast<Device*>(fnptrs);
}

CommandPool::CommandPool(Device* Vk, rc<vk::Queue> queue, u64 PoolSize)
    : PoolQueue(queue), NextBuffer(PoolSize)
{
    VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = PoolQueue->Idx,
    };

    NOSVK_ASSERT(Vk->CreateCommandPool(&info, 0, &Handle));

    std::vector<VkCommandBuffer> buf(PoolSize);
 
    VkCommandBufferAllocateInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = Handle,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = (u32)PoolSize,
    };

    NOSVK_ASSERT(Vk->AllocateCommandBuffers(&cmdInfo, buf.data()));

    Buffers.reserve(PoolSize);

    for (VkCommandBuffer cmd : buf)
    {
        Buffers.emplace_back(CommandBuffer::New(this, cmd));
    }
}

CommandPool::CommandPool(Device* Vk)
    : CommandPool(Vk, Vk->MainQueue)
{
}

Device* CommandPool::GetDevice()
{
    return PoolQueue->GetDevice();
}

CommandPool::~CommandPool()
{
    for(auto& cmd : Buffers)
        cmd->Pool = 0;

    Buffers.clear();
    GetDevice()->DestroyCommandPool(Handle, 0);
}

uint64_t NowInUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

rc<CommandBuffer> CommandPool::AllocCommandBuffer(VkCommandBufferLevel level)
{
	auto now = NowInUs();
    for (auto& cmd : Buffers)
    {
		cmd->UpdatePendingState();
    }
	bool exhausted = false;
	while (1)
	{
		auto cmd = Buffers[NextBuffer];
		if (cmd->IsFree())
			break;

		NextBuffer++;
		if (!exhausted)
		{
			auto elapsed = NowInUs() - now;
			if (elapsed > 1e4) // log if exhausted for 10ms
			{
				GLog.W("Command pool is exhausted");
				exhausted = true;
			}
		}
	}

    auto cmd = Buffers[NextBuffer];
    return cmd;
}

rc<CommandBuffer> CommandPool::BeginCmd(VkCommandBufferLevel level)
{
    rc<CommandBuffer> Cmd = AllocCommandBuffer(level);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    NOSVK_ASSERT(Cmd->Begin(&beginInfo));
    return Cmd;
}

void CommandPool::Clear()
{
    for(auto& cmd : Buffers) 
        if(cmd->State != CommandBuffer::Initial) 
            cmd->WaitAndClear();
}

} // namespace nos::vk