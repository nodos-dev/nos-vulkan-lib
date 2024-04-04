/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Allocation.h"

namespace nos::vk
{

struct BufferMemoryState
{
	VkPipelineStageFlags2 StageMask;
	VkAccessFlags2 AccessMask; // Assumes VkAccessFlagsBits same as 2
};

struct nosVulkan_API Buffer : SharedFactory<Buffer>, ResourceBase<VkBuffer>
{
	vk::Buffer* AsBuffer() override { return this; }
    VkBufferUsageFlags Usage;
	BufferMemoryState State;
    
    void Copy(size_t len, const void* pp, size_t offset = 0);

    template <class T>
    void Copy(T const& obj, size_t offset = 0)
    {
        Copy(sizeof(T), (void*)&obj, offset);
    }

    u8* Map();

    void Bind(VkDescriptorType type, u32 bind, VkDescriptorSet set);
    DescriptorResourceInfo GetDescriptorInfo() const;

    Buffer(Device* Vk, BufferCreateInfo const& info);
    ~Buffer();

    void Upload(rc<CommandBuffer> Cmd, rc<Buffer> Buffer, const VkBufferCopy* Region = 0);

	void Transition(rc<CommandBuffer> cmd, BufferMemoryState dst, VkDeviceSize offset, VkDeviceSize size);
	uint32_t Alignment;
	int ElementType;
};

} // namespace nos::vk