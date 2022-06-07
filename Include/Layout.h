#pragma once

#include <Binding.h>

#include <Device.h>

#include <Image.h>

namespace mz::vk
{

template <class T>
concept TypeClassString = std::same_as<T, std::string> || std::same_as<T, const char*>;

struct mzVulkan_API DescriptorLayout : SharedFactory<DescriptorLayout>
{
    Device* Vk;

    VkDescriptorSetLayout Handle;

    std::map<u32, NamedDSLBinding> Bindings;

    NamedDSLBinding const& operator[](u32 binding) const;

    auto begin() const
    {
        return Bindings.begin();
    }

    auto end() const
    {
        return Bindings.end();
    }

    DescriptorLayout(Device* Vk, std::map<u32, NamedDSLBinding> NamedBindings);
    ~DescriptorLayout();
};

struct mzVulkan_API DescriptorPool : SharedFactory<DescriptorPool>
{
    struct PipelineLayout* Layout;

    VkDescriptorPool Handle;

    std::vector<VkDescriptorPoolSize> Sizes;

    DescriptorPool(PipelineLayout* Layout);
    DescriptorPool(PipelineLayout* Layout, std::vector<VkDescriptorPoolSize> Sizes);
    ~DescriptorPool();
};

struct mzVulkan_API DescriptorSet : SharedFactory<DescriptorSet>
{
    DescriptorPool* Pool;
    DescriptorLayout* Layout;
    u32 Index;

    VkDescriptorSet Handle;

    DescriptorSet(DescriptorPool*, u32);

    ~DescriptorSet();

    VkDescriptorType GetType(u32 Binding);

    rc<DescriptorSet> Update(rc<CommandBuffer> Cmd, View<Binding> Res);

    void Bind(rc<CommandBuffer> Cmd);
};

struct mzVulkan_API PipelineLayout : SharedFactory<PipelineLayout>
{
    Device* Vk;

    VkPipelineLayout Handle;

    rc<DescriptorPool> Pool;

    u32 PushConstantSize;
    u32 RTCount;

    std::map<u32, rc<DescriptorLayout>> DescriptorSets;
    std::unordered_map<std::string, ShaderLayout::Index> BindingsByName;

    DescriptorLayout const& operator[](u32 set) const;

    auto begin() const
    {
        return DescriptorSets.begin();
    }

    auto end() const
    {
        return DescriptorSets.end();
    }

    ~PipelineLayout();
    rc<DescriptorSet> AllocateSet(u32 set);
    void Dump();

    PipelineLayout(Device* Vk, View<u8> src);

  private:
    PipelineLayout(Device* Vk, ShaderLayout layout);
};

} // namespace mz::vk