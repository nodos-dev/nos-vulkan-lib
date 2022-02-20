#pragma once

#include "Image.h"

namespace mz
{

template <class Resource>
requires(std::is_same_v<Resource, VulkanBuffer> || std::is_same_v<Resource, VulkanImage>) struct Binding
{
    Resource* resource;
    u32       binding;

    DescriptorResourceInfo info;

    Binding(Resource& res, u32 binding)
        : resource(&res), binding(binding)
    {
    }

    Binding(std::shared_ptr<Resource> res, u32 binding)
        : resource(res.get()), binding(binding)
    {
    }

    Binding(Resource* res, u32 binding)
        : resource(res), binding(binding)
    {
    }

    VkWriteDescriptorSet GetDescriptorInfo(VkDescriptorSet set, VkDescriptorType type)
    {
        info = resource->GetDescriptorInfo();

        return VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pImageInfo      = (std::is_same_v<Resource, VulkanImage> ? (&info.image) : 0),
            .pBufferInfo     = (std::is_same_v<Resource, VulkanBuffer> ? (&info.buffer) : 0),
        };
    }
};

struct DescriptorLayout : std::enable_shared_from_this<DescriptorLayout>
{
    VulkanDevice* Vk;

    VkDescriptorSetLayout Handle;

    std::vector<VkDescriptorSetLayoutBinding> Bindings;

    DescriptorLayout(VulkanDevice* Vk, std::vector<VkDescriptorSetLayoutBinding> bindings)
        : Vk(Vk), Bindings(std::move(bindings))
    {
        VkDescriptorSetLayoutCreateInfo info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = (u32)Bindings.size(),
            .pBindings    = Bindings.data(),
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateDescriptorSetLayout(&info, 0, &Handle));
    }

    ~DescriptorLayout()
    {
        Vk->DestroyDescriptorSetLayout(Handle, 0);
    }
};

struct DescriptorPool : std::enable_shared_from_this<DescriptorPool>
{
    struct PipelineLayout* Layout;

    VkDescriptorPool Handle;

    std::vector<VkDescriptorPoolSize> Sizes;

    DescriptorPool(PipelineLayout* Layout);
    DescriptorPool(PipelineLayout* Layout, std::vector<VkDescriptorPoolSize> Sizes);

    ~DescriptorPool();
};

struct DescriptorSet : std::enable_shared_from_this<DescriptorSet>
{
    DescriptorPool*   Pool;
    DescriptorLayout* Layout;
    u32               Index;

    VkDescriptorSet Handle;

    DescriptorSet(DescriptorPool*, u32);
    ~DescriptorSet();

    VkDescriptorType GetType(u32 Binding);

    template <class... Resource>
    std::shared_ptr<DescriptorSet> UpdateWith(Binding<Resource>... res)
    {
        VkWriteDescriptorSet writes[sizeof...(Resource)] = {res.GetDescriptorInfo(Handle, GetType(res.binding))...};

        Layout->Vk->UpdateDescriptorSets(sizeof...(Resource), writes, 0, 0);

        return shared_from_this();
    }
};

struct PipelineLayout : std::enable_shared_from_this<PipelineLayout>
{
    VulkanDevice* Vk;

    VkPipelineLayout Handle;

    std::shared_ptr<DescriptorPool> Pool;

    VkShaderStageFlags Stage;
    u32                PushConstantSize;

    std::map<u32, std::shared_ptr<DescriptorLayout>> Descriptors;

    PipelineLayout(VulkanDevice* Vk, const u32* src, u64 sz);
    PipelineLayout(VulkanDevice* Vk, std::map<u32, std::vector<VkDescriptorSetLayoutBinding>> layouts);

    ~PipelineLayout()
    {
        Vk->DestroyPipelineLayout(Handle, 0);
    }

    std::shared_ptr<DescriptorSet> AllocateSet(u32 set)
    {
        return std::make_shared<DescriptorSet>(Pool.get(), set);
    }
    void Dump();
};
} // namespace mz