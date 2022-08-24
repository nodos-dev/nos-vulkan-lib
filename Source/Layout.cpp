
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
    : DeviceChild(Vk), Bindings(std::move(NamedBindings))
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(Bindings.size());

    for (auto& [i, b] : Bindings)
    {
        VkSampler  sampler = 0;

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

rc<DescriptorSet> DescriptorSet::Update(std::map<u32, Binding> const& Res)
{
    BindStates.clear();
    std::vector<DescriptorResourceInfo> infos;
    std::vector<VkWriteDescriptorSet> writes;
    infos.reserve(Res.size());
    writes.reserve(Res.size());

    for (auto [_,res] : Res)
    {
        infos.push_back(res.GetDescriptorInfo(GetType(res.Idx)));
        writes.push_back(VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = Handle,
            .dstBinding      = res.Idx,
            .descriptorCount = 1,
            .descriptorType  = GetType(res.Idx),
            .pImageInfo      = (std::get_if<rc<ImageView>>(&res.Resource) ? &infos.back().Image : 0),
            .pBufferInfo     = (std::get_if<rc<Buffer>>(&res.Resource) ? &infos.back().Buffer : 0),
        });

        if (rc<ImageView> const* ppImg = std::get_if<rc<ImageView>>(&res.Resource))
        {
            BindStates[(*ppImg)->Src->shared_from_this()] = ImageState{
                                          .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                          .AccessMask = res.AccessFlags,
                                          .Layout = infos.back().Image.imageLayout,
            };
        }
    }
    Layout->Vk->UpdateDescriptorSets(writes.size(), writes.data(), 0, 0);
    return shared_from_this();
}

rc<DescriptorSet> DescriptorSet::Update(View<Binding> Res)
{
    BindStates.clear();
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
            .pImageInfo      = (std::get_if<rc<ImageView>>(&res.Resource) ? &infos.back().Image : 0),
            .pBufferInfo     = (std::get_if<rc<Buffer>>(&res.Resource) ? &infos.back().Buffer : 0),
        });
        if (rc<ImageView> const* ppImg = std::get_if<rc<ImageView>>(&res.Resource))
        {
            BindStates[(*ppImg)->Src->shared_from_this()] = ImageState{
                                          .StageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                          .AccessMask = res.AccessFlags,
                                          .Layout = infos.back().Image.imageLayout,
            };
        }
    }
    Layout->Vk->UpdateDescriptorSets(writes.size(), writes.data(), 0, 0);
    return shared_from_this();
}

void DescriptorSet::Bind(rc<CommandBuffer> Cmd)
{
    for (auto [img, state] : BindStates)
    {
        img->Transition(Cmd, state);
    }
    Cmd->AddDependency(shared_from_this());
    Cmd->BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, Pool->Layout->Handle, Index, 1, &Handle, 0, 0);
}

DescriptorSet::DescriptorSet(DescriptorPool* pool, u32 Index)
    : Pool(pool), Layout(pool->Layout->DescriptorLayouts[Index].get()), Index(Index)
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
    Pool->InUse--;
    if (Pool->Prev && !Pool->InUse)
    {
        Pool->Prev->Next = Pool->Next;
    }
}

VkDescriptorType DescriptorSet::GetType(u32 Binding)
{
    return Layout->Bindings[Binding].DescriptorType;
}

static std::vector<VkDescriptorPoolSize> GetPoolSizes(PipelineLayout* Layout)
{
    std::map<VkDescriptorType, u32> counter;

    for (auto& [_, set] : Layout->DescriptorLayouts)
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
    : Layout(Layout), Sizes(std::move(sizes)), MaxSets((u32)Layout->DescriptorLayouts.size() * 1024u)
{
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = MaxSets.load(),
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

PipelineLayout::PipelineLayout(Device* Vk, ShaderLayout layout)
    : DeviceChild(Vk), PushConstantSize(layout.PushConstantSize), RTCount(layout.RTCount), BindingsByName(std::move(layout.BindingsByName))
{
    std::vector<VkDescriptorSetLayout> handles;

    VkPushConstantRange pushConstantRange = {
        .offset = 0,
        .size   = PushConstantSize,
    };

    for (auto& [idx, set] : layout.DescriptorSets)
    {
        for (auto& [_, binding] : set)
        {
            pushConstantRange.stageFlags |= binding.StageMask;
        }

        auto layout = DescriptorLayout::New(Vk, std::move(set));
        handles.push_back(layout->Handle);
        DescriptorLayouts[idx] = layout;
    }

   for (auto& [set, layout] : *this)
    {
        for (auto& [binding, dsl] : *layout)
        {
            u32 shift = UniformSize % dsl.Type->Alignment;
            if (shift)
            {
                UniformSize += dsl.Type->Alignment - shift;
            }
            OffsetMap[((u64)set << 32ull) | binding] = UniformSize;
            UniformSize += dsl.Type->Size;
        }
    }

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = (u32)handles.size(),
        .pSetLayouts            = handles.data(),
        .pushConstantRangeCount = layout.PushConstantSize ? 1u : 0u,
        .pPushConstantRanges    = layout.PushConstantSize ? &pushConstantRange : 0,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreatePipelineLayout(&layoutInfo, 0, &Handle));
}

void PipelineLayout::Dump()
{
    for (auto& [set, layout] : DescriptorLayouts)
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
    return *DescriptorLayouts.at(set);
}

PipelineLayout::~PipelineLayout()
{
    Vk->DestroyPipelineLayout(Handle, 0);
}

rc<DescriptorPool> PipelineLayout::CreatePool()
{
    return DescriptorPool::New(this);
}

rc<DescriptorSet> DescriptorPool::AllocateSet(u32 set)
{
    if (MaxSets == InUse)
    {
        if (!Next)
        {
            Next = DescriptorPool::New(Layout);
            Next->Prev = this;
        }
        return Next->AllocateSet(set);
    }
    InUse++;
    return DescriptorSet::New(this, set);
}

} // namespace mz::vk