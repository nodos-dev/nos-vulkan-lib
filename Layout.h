#pragma once

#include "Device.h"
#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <memory>

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
            .bindingCount = (u32)bindings.size(),
            .pBindings    = bindings.data(),
        };

        CHECKRE(Vk->CreateDescriptorSetLayout(&info, 0, &Handle));
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

    VkDescriptorSet Handle;

    DescriptorSet(DescriptorPool*, DescriptorLayout*);
    ~DescriptorSet();
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
        return std::make_shared<DescriptorSet>(Pool.get(), Descriptors[set].get());
    }

    void Dump();
};
