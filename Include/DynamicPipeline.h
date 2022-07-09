
#pragma once

#include <Layout.h>
#include <Shader.h>
#include <Command.h>

namespace mz::vk
{

struct mzVulkan_API DynamicPipeline : SharedFactory<DynamicPipeline>, DeviceChild
{
    inline static VertexShader* VS = nullptr;
    rc<Shader> PS;
    rc<PipelineLayout> Layout;
    VkPipeline Handle = 0;

    rc<ImageView> RenderTarget;

    std::vector<rc<DescriptorSet>> DescriptorSets;

    DynamicPipeline(Device* Vk, View<u8> src, VkSampler sampler = 0);

    DynamicPipeline(Device* Vk, VkExtent2D extent, View<u8> src, VkSampler sampler = 0, std::vector<VkFormat> fmt = {});
    ~DynamicPipeline();

    VertexShader* GetVS() const;
    void ChangeTarget(rc<ImageView> Image);
    void CreateWithImage(rc<ImageView> Image);

    void BeginRendering(rc<CommandBuffer> Cmd, rc<ImageView> Image = 0);


    template <class T>
    void PushConstants(rc<CommandBuffer> Cmd, T const& data)
    {
        Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
    }

    bool BindResources(rc<CommandBuffer> Cmd, std::unordered_map<std::string, Binding::Type> const& resources);
    void BindResources(rc<CommandBuffer> Cmd, std::map<u32, std::vector<Binding>> const& bindings);

    template <class... Args>
    requires(StringResourcePairPack<std::remove_cvref_t<Args>...>()) bool BindResources(rc<CommandBuffer> Cmd, Args&&... args)
    {
        std::map<u32, std::vector<Binding>> bindings;
        if (!Insert(bindings, std::forward<Args>(args)...))
        {
            return false;
        }
        BindResources(Cmd, bindings);
        return true;
    }

    template <class K, class V, class... Rest>
    bool Insert(std::map<u32, std::vector<Binding>>& bindings, K&& k, V&& v, Rest&&... rest)
    {
        auto it = Layout->BindingsByName.find(k);
        if (it == Layout->BindingsByName.end())
        {
            return false;
        }
        bindings[it->second.set].push_back(Binding(v, it->second.binding));
        if constexpr (sizeof...(rest) > 0)
        {
            return Insert(bindings, std::forward<Rest>(rest)...);
        }
        return true;
    }

  private:
    template <class A, class B, class... Tail>
    inline static constexpr bool StringResourcePairPack()
    {
        if constexpr ((sizeof...(Tail) % 2 == 0) && std::convertible_to<A, std::string> && TypeClassResource<B>)
        {
            if constexpr (sizeof...(Tail))
            {
                return StringResourcePairPack<Tail...>();
            }
            else
            {
                return true;
            }
        }
        return false;
    }
};
} // namespace mz::vk