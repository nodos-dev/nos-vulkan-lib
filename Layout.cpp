#include "Layout.h"

#include <memory>
#include <spirv_cross.hpp>

DescriptorSet::DescriptorSet(DescriptorPool* pool, DescriptorLayout* layout)
    : Pool(pool), Layout(layout)
{
    VkDescriptorSetAllocateInfo info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = Pool->Handle,
        .descriptorSetCount = 1,
        .pSetLayouts        = &Layout->Handle,
    };

    CHECKRE(layout->Vk->AllocateDescriptorSets(&info, &Handle));
}

DescriptorSet::~DescriptorSet()
{
    Layout->Vk->FreeDescriptorSets(Pool->Handle, 1, &Handle);
}

DescriptorPool::DescriptorPool(PipelineLayout* Layout)
    : Layout(Layout)
{
    std::map<VkDescriptorType, u32> counter;

    for (auto& [_, set] : Layout->Descriptors)
    {
        for (auto& binding : set->Bindings)
        {
            counter[binding.descriptorType] += binding.descriptorCount;
        }
    }

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(counter.size());

    for (auto& [type, count] : counter)
    {
        poolSizes.push_back(VkDescriptorPoolSize{.type = type, .descriptorCount = count * 1024});
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = (u32)Layout->Descriptors.size() * 2048u,
        .poolSizeCount = (u32)poolSizes.size(),
        .pPoolSizes    = poolSizes.data(),
    };

    CHECKRE(Layout->Vk->CreateDescriptorPool(&poolInfo, 0, &Handle));
}

DescriptorPool::~DescriptorPool()
{
    if (Handle)
    {
        Layout->Vk->DestroyDescriptorPool(Handle, 0);
    }
}

PipelineLayout::PipelineLayout(VulkanDevice* Vk, const u32* src, u64 sz)
    : Vk(Vk), Stage(0), PushConstantSize(0), Pool(0)
{

    std::vector<VkDescriptorSetLayout> handles;

    for (auto& [set, descriptor] : GetLayouts(src, sz))
    {
        auto layout = std::make_shared<DescriptorLayout>(Vk, std::move(descriptor));
        handles.push_back(layout->Handle);
        Descriptors[set] = layout;
    }

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (u32)handles.size(),
        .pSetLayouts    = handles.data(),
    };

    CHECKRE(Vk->CreatePipelineLayout(&layoutInfo, 0, &Handle));

    if (!Descriptors.empty())
    {
        Pool = std::make_shared<DescriptorPool>(this);
    }
}

void PipelineLayout::Dump()
{
    for (auto& [set, layout] : Descriptors)
    {
        printf("Set %u:\n", set);
        for (auto& binding : layout->Bindings)
        {
            printf("\t Binding %u:%s\n", binding.binding, descriptor_type_to_string(binding.descriptorType));
        }
    }
}
