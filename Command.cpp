#include "Command.h"
#include "vulkan/vulkan_core.h"

CommandBuffer::CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle)
    : VklCommandFunctions{Pool->GetDevice(), Handle}, Pool(Pool)
{
}

CommandBuffer::~CommandBuffer()
{
    Pool->Queue.WaitIdle();

    MZ_VULKAN_ASSERT_SUCCESS(Reset(VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));

    GetDevice()->FreeCommandBuffers(Pool->Handle, 1, &handle);
}
