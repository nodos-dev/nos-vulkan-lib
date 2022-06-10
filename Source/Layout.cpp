
#include <Layout.h>
#include <Command.h>
#include <Image.h>

#include <spirv_cross.hpp>

namespace mz::vk
{

NamedDSLBinding const& DescriptorLayout::operator[](u32 binding) const
{
    return Bindings.at(binding);
}

DescriptorLayout::DescriptorLayout(Device* Vk, std::map<u32, NamedDSLBinding> NamedBindings)
    : DeviceChild{.Vk = Vk}, Bindings(std::move(NamedBindings))
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(Bindings.size());

    for (auto& [i, b] : Bindings)
    {
        bindings.emplace_back(VkDescriptorSetLayoutBinding{
            .binding         = i,
            .descriptorType  = b.DescriptorType,
            .descriptorCount = b.DescriptorCount,
            .stageFlags      = b.StageMask,
        });
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (u32)bindings.size(),
        .pBindings    = bindings.data(),
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateDescriptorSetLayout(&info, 0, &Handle));
}

DescriptorLayout::~DescriptorLayout()
{
    Vk->DestroyDescriptorSetLayout(Handle, 0);
}

rc<DescriptorSet> DescriptorSet::Update(rc<CommandBuffer> Cmd, View<Binding> Res)
{
    std::vector<DescriptorResourceInfo> infos;
    std::vector<VkWriteDescriptorSet> writes;
    infos.reserve(Res.size());
    writes.reserve(Res.size());

    for (auto res : Res)
    {
        infos.push_back(res.GetDescriptorInfo(GetType(res.Idx)));
        writes.push_back(VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = Handle,
            .dstBinding      = res.Idx,
            .descriptorCount = 1,
            .descriptorType  = GetType(res.Idx),
            .pImageInfo      = (std::get_if<rc<Image>>(&res.Resource) ? &infos.back().Image : 0),
            .pBufferInfo     = (std::get_if<rc<Buffer>>(&res.Resource) ? &infos.back().Buffer : 0),
        });

        if (rc<Image> const* ppImg = std::get_if<rc<Image>>(&res.Resource))
        {
            (*ppImg)->Transition(Cmd, ImageState{
                                          .StageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                          .AccessMask = res.AccessFlags,
                                          .Layout     = infos.back().Image.imageLayout,
                                      });
        }
    }
    Layout->Vk->UpdateDescriptorSets(writes.size(), writes.data(), 0, 0);

    return shared_from_this();
}

void DescriptorSet::Bind(rc<CommandBuffer> Cmd)
{
    Cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, Pool->Layout->Handle, Index, 1, &Handle, 0, 0);
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
    return Layout->Bindings[Binding].DescriptorType;
}

static std::vector<VkDescriptorPoolSize> GetPoolSizes(PipelineLayout* Layout)
{
    std::map<VkDescriptorType, u32> counter;

    for (auto& [_, set] : Layout->DescriptorSets)
    {
        for (auto& [_, binding] : set->Bindings)
        {
            counter[binding.DescriptorType] += binding.DescriptorCount;
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

PipelineLayout::PipelineLayout(Device* Vk, View<u8> src)
    : PipelineLayout(Vk, GetShaderLayouts(src))
{
}

PipelineLayout::PipelineLayout(Device* Vk, ShaderLayout layout)
    : DeviceChild{.Vk = Vk}, PushConstantSize(layout.PushConstantSize), RTCount(layout.RTCount), Pool(0), BindingsByName(std::move(layout.BindingsByName))
{
    std::vector<VkDescriptorSetLayout> handles;

    VkPushConstantRange pushConstantRange = {
        .offset = 0,
        .size   = layout.PushConstantSize,
    };

    for (auto& [idx, set] : layout.DescriptorSets)
    {
        for (auto& [_, binding] : set)
        {
            pushConstantRange.stageFlags |= binding.StageMask;
        }

        auto layout = DescriptorLayout::New(Vk, std::move(set));
        handles.push_back(layout->Handle);
        DescriptorSets[idx] = layout;
    }

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = (u32)handles.size(),
        .pSetLayouts            = handles.data(),
        .pushConstantRangeCount = layout.PushConstantSize ? 1u : 0u,
        .pPushConstantRanges    = layout.PushConstantSize ? &pushConstantRange : 0,
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
            printf("\t Binding %s @%u:%s\n", binding.Name.c_str(), binding.Binding, descriptor_type_to_string(binding.DescriptorType));
        }
    }
}

DescriptorLayout const& PipelineLayout::operator[](u32 set) const
{
    return *DescriptorSets.at(set);
}

PipelineLayout::~PipelineLayout()
{
    Vk->DestroyPipelineLayout(Handle, 0);
}

rc<DescriptorSet> PipelineLayout::AllocateSet(u32 set)
{
    return DescriptorSet::New(Pool.get(), set);
}

} // namespace mz::vk