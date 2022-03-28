
#include <Command.h>

#include <Image.h>

namespace mz::vk
{

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

CommandBuffer::~CommandBuffer()
{
    if (!Ready())
    {
        GetDevice()->WaitForFences(1, &Fence, 1, -1);
    }
    GetDevice()->DestroyFence(Fence, 0);

    MZ_VULKAN_ASSERT_SUCCESS(Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
}

void CommandBuffer::Submit(View<VkSemaphore> Wait, View<VkPipelineStageFlags> Stages, View<VkSemaphore> Signal)
{
    assert(Wait.size() == Stages.size());

    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = (u32)Wait.size(),
        .pWaitSemaphores      = Wait.data(),
        .pWaitDstStageMask    = Stages.data(),
        .commandBufferCount   = 1,
        .pCommandBuffers      = &handle,
        .signalSemaphoreCount = (u32)Signal.size(),
        .pSignalSemaphores    = Signal.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(End());

    // MZ_VULKAN_ASSERT_SUCCESS(Pool->Queue.Submit(1, &submitInfo, 0));
    MZ_VULKAN_ASSERT_SUCCESS(Pool->Queue.Submit(1, &submitInfo, Fence));
}

void CommandBuffer::Submit(rc<Image> image, VkPipelineStageFlags stage)
{
    VkSemaphore sem[1]             = {image->Sema};
    VkPipelineStageFlags stages[1] = {stage};
    Submit(sem, stages, sem);
}

void CommandBuffer::Submit(View<rc<Image>> images, VkPipelineStageFlags stage)
{
    std::vector<VkSemaphore> semaphores = TransformView<VkSemaphore, rc<Image>>(images, [](auto img) { return img->Sema; }).collect();
    Submit(semaphores, std::vector<VkPipelineStageFlags>(images.size(), stage), semaphores);
}

bool CommandBuffer::Ready()
{
    return VK_SUCCESS == GetDevice()->GetFenceStatus(Fence);
}

void CommandBuffer::Wait()
{
    MZ_VULKAN_ASSERT_SUCCESS(GetDevice()->WaitForFences(1, &Fence, 0, -1));

    for (auto& fn : Callbacks)
    {
        fn();
    }

    Callbacks.clear();
}

Device* CommandBuffer::GetDevice()
{
    return static_cast<Device*>(fnptrs);
}

CommandPool::CommandPool(Device* Vk, u32 family)
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

Device* CommandPool::GetDevice()
{
    return Queue.GetDevice();
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

    assert(cmd->Ready());

    GetDevice()->ResetFences(1, &cmd->Fence);
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