#pragma once

#include <Binding.h>

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

    std::set<Binding> Bound;

    VkDescriptorType GetType(u32 Binding);

    template <std::same_as<Binding>... Bindings>
    std::shared_ptr<DescriptorSet> UpdateWith(Bindings&&... res)
    {
        VkWriteDescriptorSet writes[sizeof...(Bindings)] = {res.GetDescriptorInfo(Handle, GetType(res.binding))...};
        Layout->Vk->UpdateDescriptorSets(sizeof...(Bindings), writes, 0, 0);
        Bound.insert(res...);
        return shared_from_this();
    }

    std::shared_ptr<DescriptorSet> UpdateWith(View<Binding> res);
    std::shared_ptr<DescriptorSet> Bind(std::shared_ptr<CommandBuffer> Cmd);
};

struct mzVulkan_API PipelineLayout : SharedFactory<PipelineLayout>
{
    Device* Vk;

    VkPipelineLayout Handle;

    std::shared_ptr<DescriptorPool> Pool;

    u32 PushConstantSize;
    u32 RTCount;

    std::map<u32, std::shared_ptr<DescriptorLayout>> DescriptorSets;
    std::unordered_map<std::string, glm::uvec2> BindingsByName;

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
    std::shared_ptr<DescriptorSet> AllocateSet(u32 set);
    void Dump();

    PipelineLayout(Device* Vk, View<u8> src);

  private:
    PipelineLayout(Device* Vk, ShaderLayout layout);
};

} // namespace mz::vk