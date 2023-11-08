/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Allocation.h"

namespace mz::vk
{

struct mzVulkan_API Buffer : SharedFactory<Buffer>, ResourceBase<VkBuffer>
{
    VkBufferUsageFlags Usage;
    
    void Copy(size_t len, void* pp, size_t offset = 0);

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

};

} // namespace mz::vk