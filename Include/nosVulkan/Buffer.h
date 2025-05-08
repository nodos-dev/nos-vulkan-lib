/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Allocation.h"

namespace nos::vk
{

struct BufferMemoryState
{
	VkPipelineStageFlags2 StageMask;
	VkAccessFlags2 AccessMask; // Assumes VkAccessFlagsBits same as 2
	uint32_t QueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
	rc<CommandBuffer> PreviousCmd = nullptr;
};

struct nosVulkan_API BufferCreationInfos
{
	uint32_t MemoryTypeIndex = UINT32_MAX;
	uint32_t ExtMemHandleType = 0;
	VmaAllocationCreateInfo AllocCreateInfo{};
	VkBufferCreateInfo BufCreateInfo{};
	VkExternalMemoryBufferCreateInfo ExtMemCreateInfo{};
	VkMemoryPropertyFlags MemProps = 0;
	BufferCreationInfos(BufferCreationInfos&& o) noexcept;
	BufferCreationInfos() = default;
	BufferCreationInfos(BufferCreationInfos const& o) = delete;
	BufferCreationInfos& operator=(const BufferCreationInfos&) = delete;
};

Result<BufferCreationInfos> nosVulkan_API CalculateBufferCreationInfos(vk::Device* device, BufferCreateRequest const& request);

BufferCreateRequest nosVulkan_API GetBufferCreateRequestForTempUploadBuffer(uint64_t size);

struct nosVulkan_API Buffer : SharedFactory<Buffer>, ResourceBase<VkBuffer>
{
protected:
    Buffer(Device* device, VkBuffer buffer, VkBufferUsageFlags usage, uint32_t alignment, int elementType, std::optional<Allocation> alloc, VkDeviceSize size);
public:
	static Result<rc<Buffer>> Create(Device* device, BufferCreateRequest const& request, VkResult* outVkRes = nullptr);
	static rc<Buffer> FromExisting(Device* device, VkBuffer buffer, VkBufferUsageFlags usage, uint32_t alignment, int elementType, std::optional<Allocation> alloc, VkDeviceSize size);
	static Result<BufferCreateRequest> TryGetRelaxedSuitableCreateRequest(Device* Vk, BufferCreateRequest const& info);
	static Result<rc<Buffer>> CreateRelaxed(Device* Vk, BufferCreateRequest const& createInfo, VkResult* vkRes = nullptr);
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

    ~Buffer();

    void Upload(rc<CommandBuffer> Cmd, rc<Buffer> Buffer, const VkBufferCopy* Region = 0);

	void Transition(rc<CommandBuffer> curCmd, BufferMemoryState dst, VkDeviceSize offset, VkDeviceSize size);
	uint32_t Alignment;
	int ElementType;
};

} // namespace nos::vk