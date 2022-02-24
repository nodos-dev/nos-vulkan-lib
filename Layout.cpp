#include "Layout.h"

#include <spirv_cross.hpp>

namespace mz
{
DescriptorSet::DescriptorSet(DescriptorPool* pool, u32 Index)
    : Pool(pool), Layout(pool->Layout->Descriptors[Index].get()), Index(Index)
{
    VkDescriptorSetAllocateInfo info = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = Pool->Handle,
        .descriptorSetCount = 1,
        .pSetLayouts        = &Layout->Handle,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Layout->Vk->AllocateDescriptorSets(&info, &Handle));
}

DescriptorSet::~DescriptorSet()
{
    Pool->Layout->Vk->FreeDescriptorSets(Pool->Handle, 1, &Handle);
}

VkDescriptorType DescriptorSet::GetType(u32 Binding)
{
    return Layout->Bindings[Binding].descriptorType;
}

std::vector<VkDescriptorPoolSize> GetPoolSizes(PipelineLayout* Layout)
{
    std::map<VkDescriptorType, u32> counter;

    for (auto& [_, set] : Layout->Descriptors)
    {
        for (auto& binding : set->Bindings)
        {
            counter[binding.descriptorType] += binding.descriptorCount;
        }
    }

    std::vector<VkDescriptorPoolSize> Sizes;

    Sizes.reserve(counter.size());

    for (auto& [type, count] : counter)
    {
        Sizes.push_back(VkDescriptorPoolSize{.type = type, .descriptorCount = count * 1024});
    }
    return Sizes;
}

DescriptorPool::DescriptorPool(PipelineLayout* Layout)
    : DescriptorPool(Layout, GetPoolSizes(Layout))
{
}

DescriptorPool::DescriptorPool(PipelineLayout* Layout, std::vector<VkDescriptorPoolSize> sizes)
    : Layout(Layout), Sizes(std::move(sizes))
{

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = (u32)Layout->Descriptors.size() * 2048u,
        .poolSizeCount = (u32)Sizes.size(),
        .pPoolSizes    = Sizes.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(Layout->Vk->CreateDescriptorPool(&poolInfo, 0, &Handle));
}

DescriptorPool::~DescriptorPool()
{
    if (Handle)
    {
        Layout->Vk->DestroyDescriptorPool(Handle, 0);
    }
}

PipelineLayout::PipelineLayout(VulkanDevice* Vk, const u32* src, u64 sz)
    : PipelineLayout(Vk, GetLayouts(src, sz, RTcount))
{
}

PipelineLayout::PipelineLayout(VulkanDevice* Vk, std::map<u32, std::vector<VkDescriptorSetLayoutBinding>> layouts)
    : Vk(Vk), PushConstantSize(0), Pool(0)
{
    std::vector<VkDescriptorSetLayout> handles;

    for (auto& [set, descriptor] : layouts)
    {
        auto layout = std::make_shared<DescriptorLayout>(Vk, std::move(descriptor));
        handles.push_back(layout->Handle);
        Descriptors[set] = layout;
    }

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = 256,
    };

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = (u32)handles.size(),
        .pSetLayouts            = handles.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushConstantRange,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreatePipelineLayout(&layoutInfo, 0, &Handle));

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
} // namespace mz