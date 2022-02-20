
#include "Command.h"

#include "Image.h"

namespace mz
{
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

    MZ_VULKAN_ASSERT_SUCCESS(Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));

    GetDevice()->DestroyFence(Fence, 0);
}

void CommandBuffer::Submit(
    uint32_t                    waitSemaphoreCount,
    const VkSemaphore*          pWaitSemaphores,
    const VkPipelineStageFlags* pWaitDstStageMask,
    uint32_t                    signalSemaphoreCount,
    const VkSemaphore*          pSignalSemaphores)
{
    MZ_VULKAN_ASSERT_SUCCESS(End());

    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = waitSemaphoreCount,
        .pWaitSemaphores      = pWaitSemaphores,
        .pWaitDstStageMask    = pWaitDstStageMask,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &handle,
        .signalSemaphoreCount = signalSemaphoreCount,
        .pSignalSemaphores    = pSignalSemaphores,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Pool->Queue.Submit(1, &submitInfo, Fence));
}

void CommandBuffer::Submit(std::shared_ptr<VulkanImage> image, VkPipelineStageFlags stage)
{

    Submit(1, &image->Sema, &stage, 1, &image->Sema);
}

} // namespace mz