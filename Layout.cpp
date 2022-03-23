#include "Layout.h"
#include "Command.h"

#include <algorithm>
#include <memory>
#include <spirv_cross.hpp>

namespace mz::vk
{

std::shared_ptr<DescriptorSet> DescriptorSet::Bind(std::shared_ptr<CommandBuffer> Cmd)
{
    for (auto& res : Bound)
    {
        if (Image* const* ppimage = std::get_if<Image*>(&res.resource))
        {
            (**ppimage).Transition(Cmd, res.info.image.imageLayout, res.access);
        }
    }
    Cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, Pool->Layout->Handle, Index, 1, &Handle, 0, 0);

    return shared_from_this();
}

DescriptorSet::DescriptorSet(DescriptorPool* pool, u32 Index)
    : Pool(pool), Layout(pool->Layout->DescriptorSets[Index].get()), Index(Index)
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

    for (auto& [_, set] : Layout->DescriptorSets)
    {
        for (auto& [_, binding] : set->Bindings)
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
        .maxSets       = (u32)Layout->DescriptorSets.size() * 2048u,
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

PipelineLayout::PipelineLayout(Device* Vk, vkView<u8> src)
    : PipelineLayout(Vk, GetShaderLayouts(src))
{
}

PipelineLayout::PipelineLayout(Device* Vk, ShaderLayout layout)
    : Vk(Vk), PushConstantSize(layout.PushConstantSize), RTCount(layout.RTCount), Pool(0), BindingsByName(std::move(layout.BindingsByName))
{
    std::vector<VkDescriptorSetLayout> handles;

    for (auto& [idx, set] : layout.DescriptorSets)
    {
        auto layout = DescriptorLayout::New(Vk, std::move(set));
        handles.push_back(layout->Handle);
        DescriptorSets[idx] = layout;
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

    if (!DescriptorSets.empty())
    {
        Pool = DescriptorPool::New(this);
    }
}

void PipelineLayout::Dump()
{
    for (auto& [set, layout] : DescriptorSets)
    {
        printf("Set %u:\n", set);
        for (auto& [_, binding] : layout->Bindings)
        {
            printf("\t Binding %s @%u:%s\n", binding.name.c_str(), binding.binding, descriptor_type_to_string(binding.descriptorType));
        }
    }
}
} // namespace mz::vk