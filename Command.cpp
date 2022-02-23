
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

void CommandBuffer::Submit1()
{
    MZ_VULKAN_ASSERT_SUCCESS(End());

    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &handle,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Pool->Queue.Submit(1, &submitInfo, Fence));
}

void CommandBuffer::Submit2(std::vector<VulkanImage*> images, VkPipelineStageFlags stage)
{
    std::vector<VkSemaphore>          semaphores;
    std::vector<VkPipelineStageFlags> stages;

    std::vector<u64> wait;
    std::vector<u64> signal;

    semaphores.reserve(images.size());
    stages.reserve(images.size());
    wait.reserve(images.size());
    signal.reserve(images.size());

    for (auto img : images)
    {
        u64 val = img->AcquireValue();

        semaphores.push_back(img->Sema);
        stages.push_back(stage);
        wait.push_back(val);
        signal.push_back(val + 1);
    }

    u32 count = images.size();

    VkTimelineSemaphoreSubmitInfo timelineInfo = {
        .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .waitSemaphoreValueCount   = count,
        .pWaitSemaphoreValues      = wait.data(),
        .signalSemaphoreValueCount = count,
        .pSignalSemaphoreValues    = signal.data(),
    };

    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = &timelineInfo,
        .waitSemaphoreCount   = count,
        .pWaitSemaphores      = semaphores.data(),
        .pWaitDstStageMask    = &stage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &handle,
        .signalSemaphoreCount = count,
        .pSignalSemaphores    = semaphores.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(End());
    MZ_VULKAN_ASSERT_SUCCESS(Pool->Queue.Submit(1, &submitInfo, 0));
}

void CommandBuffer::Submit2(VulkanImage* image, VkPipelineStageFlags stage)
{
    u64 val  = image->AcquireValue();
    u64 sval = val + 1;

    VkTimelineSemaphoreSubmitInfo timelineInfo = {
        .sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .waitSemaphoreValueCount   = 1,
        .pWaitSemaphoreValues      = &val,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues    = &sval,
    };

    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = &timelineInfo,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &image->Sema,
        .pWaitDstStageMask    = &stage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &handle,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &image->Sema,
    };

    MZ_VULKAN_ASSERT_SUCCESS(End());
    MZ_VULKAN_ASSERT_SUCCESS(Pool->Queue.Submit(1, &submitInfo, 0));
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

// void CommandBuffer::Submit(VulkanImage* image, VkPipelineStageFlags stage)
// {
//     Submit(1, &image->Sema, &stage, 1, &image->Sema);
// }

// void CommandBuffer::Submit(std::vector<VulkanImage*> images, VkPipelineStageFlags stage)
// {
//     std::vector<VkSemaphore>          semaphores;
//     std::vector<VkPipelineStageFlags> stages;

//     semaphores.reserve(images.size());
//     stages.reserve(images.size());

//     for (auto img : images)
//     {
//         semaphores.push_back(img->Sema);
//         stages.push_back(stage);
//     }

//     Submit(semaphores.size(), semaphores.data(), stages.data(), semaphores.size(), semaphores.data());
// }

} // namespace mz