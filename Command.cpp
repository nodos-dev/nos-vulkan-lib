#include "Command.h"
#include "vulkan/vulkan_core.h"

CommandBuffer::CommandBuffer(CommandPool* Pool, VkCommandBuffer Handle)
    : VklCommandFunctions{Pool->GetDevice(), Handle}, Pool(Pool)
{
}

CommandBuffer::~CommandBuffer()
{
    GetDevice()->FreeCommandBuffers(Pool->Handle, 1, &handle);
}
