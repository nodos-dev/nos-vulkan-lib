
#include "Command.h"

#include "Image.h"

namespace mz::vk
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

void CommandBuffer::Submit(std::shared_ptr<Image> image, VkPipelineStageFlags stage)
{
    Submit(image.get(), stage);
}

void CommandBuffer::Submit(Image* image, VkPipelineStageFlags stage)
{
    VkSemaphore sem[1]             = {image->Sema};
    VkPipelineStageFlags stages[1] = {stage};
    Submit(sem, stages, sem);
}

void CommandBuffer::Submit(View<std::shared_ptr<Image>> images, VkPipelineStageFlags stage)
{
    std::vector<VkSemaphore> semaphores = TransformView<VkSemaphore, std::shared_ptr<Image>>(images, [](auto img) { return img->Sema; }).collect();
    Submit(semaphores, std::vector<VkPipelineStageFlags>(images.size(), stage), semaphores);
}

} // namespace mz::vk