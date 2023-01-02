/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Binding.h"
#include "Device.h"
#include "Image.h"

namespace mz::vk
{

template <class T>
concept TypeClassString = std::same_as<T, std::string> || std::same_as<T, const char*>;

struct mzVulkan_API DescriptorLayout : SharedFactory<DescriptorLayout>, DeviceChild
{
    VkDescriptorSetLayout Handle;
    std::map<u32, NamedDSLBinding> Bindings;
    NamedDSLBinding const& operator[](u32 binding) const;
    DescriptorLayout(Device* Vk, std::map<u32, NamedDSLBinding> NamedBindings);
    ~DescriptorLayout();
    
    auto begin() const
    {
        return Bindings.begin();
    }

    auto end() const
    {
        return Bindings.end();
    }
};

struct mzVulkan_API DescriptorPool : SharedFactory<DescriptorPool>
{
    std::mutex Mutex;
    rc<struct PipelineLayout> Layout;
    VkDescriptorPool Handle;
    std::vector<VkDescriptorPoolSize> Sizes;
    std::atomic_uint InUse = 0;
    std::atomic_uint MaxSets = 0;
    DescriptorPool(rc<PipelineLayout> Layout);
    DescriptorPool(rc<PipelineLayout> Layout, std::vector<VkDescriptorPoolSize> Sizes);
    ~DescriptorPool();
    rc<DescriptorPool> Next = 0;
    DescriptorPool* Prev = 0;
    rc<struct DescriptorSet> AllocateSet(u32 set);
};

struct mzVulkan_API DescriptorSet : SharedFactory<DescriptorSet>
{
    rc<DescriptorPool> Pool;
    DescriptorLayout* Layout;
    u32 Index;
    VkDescriptorSet Handle;
    std::unordered_map<rc<Image>, ImageState> BindStates;
    DescriptorSet(rc<DescriptorPool>, u32);
    ~DescriptorSet();
    VkDescriptorType GetType(u32 Binding);
    rc<DescriptorSet> Update(View<Binding> Res);
    rc<DescriptorSet> Update(std::map<u32, Binding> const& Res);
    void Bind(rc<CommandBuffer> Cmd, VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
};

struct mzVulkan_API PipelineLayout : SharedFactory<PipelineLayout>, DeviceChild
{
    VkPipelineLayout Handle;
    
    u32 PushConstantSize = 0;
    u32 RTCount = 1;
    u32 UniformSize = 0;

    std::map<u64, u32> OffsetMap;
    std::map<u32, rc<DescriptorLayout>> DescriptorLayouts;
    std::unordered_map<std::string, ShaderLayout::Index> BindingsByName;
    PipelineLayout(Device* Vk, ShaderLayout layout);
    ~PipelineLayout();
    
    rc<DescriptorPool> CreatePool();

    void Dump();

    NamedDSLBinding const& operator[](ShaderLayout::Index) const;
    DescriptorLayout const& operator[](u32 set) const;
    ShaderLayout::Index operator[](std::string const& name) const;

    auto begin() const
    {
        return DescriptorLayouts.begin();
    }

    auto end() const
    {
        return DescriptorLayouts.end();
    }
};

} // namespace mz::vk