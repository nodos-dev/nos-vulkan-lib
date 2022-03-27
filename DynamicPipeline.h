
#pragma once

#include <Layout.h>
#include <Shader.h>

namespace mz::vk
{

struct mzVulkan_API DynamicPipeline : SharedFactory<DynamicPipeline>
{
    Device* Vk;

    rc<Shader> Shader;
    rc<PipelineLayout> Layout;

    VkPipeline Handle;

    VkExtent2D Extent;

    DynamicPipeline(Device* Vk, VkExtent2D extent, View<u8> src);

    ~DynamicPipeline();

    void BeginWithRTs(rc<CommandBuffer> Cmd, View<rc<Image>> Images);

    template <std::same_as<rc<vk::Image>>... RT>
    void BeginWithRTs(rc<CommandBuffer> Cmd, RT... Images)
    {
        rc<vk::Image> RTs[] = {Images...};
        BeginWithRTs(Cmd, RTs);
    }

    template <class T>
    void PushConstants(rc<CommandBuffer> Cmd, T const& data)
    {
        Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
    }

    // bool BindResources(rc<CommandBuffer> Cmd, std::unordered_map<std::string, Binding::Type> const& resources);

    template <class...>
    static constexpr bool str_res_pair = false;

    template <std::convertible_to<std::string> A, TypeClassResource B, class... Tail>
    static constexpr bool str_res_pair<A, B, Tail...> = str_res_pair<Tail...>;

    template <>
    static constexpr bool str_res_pair<> = true;

    template <class... Args>
    requires(str_res_pair<std::remove_cvref_t<Args>...>) bool BindResources(rc<CommandBuffer> Cmd, Args&&... args)
    {
        std::map<u32, std::vector<Binding>> bindings;
        if (!Insert(bindings, std::forward<Args>(args)...))
        {
            return false;
        }
        for (auto& [idx, set] : bindings)
        {
            auto dset = Layout->AllocateSet(idx)->UpdateWith(set)->Bind(Cmd);
            Cmd->Callbacks.push_back([dset]() {});
        }
        return true;
    }

  private:
    template <class K, class V, class... Rest>
    bool Insert(std::map<u32, std::vector<Binding>>& bindings, K&& k, V&& v, Rest&&... rest)
    {
        auto it = Layout->BindingsByName.find(k);
        if (it == Layout->BindingsByName.end())
        {
            return false;
        }
        bindings[it->second.x].push_back(Binding(v, it->second.y));
        if constexpr (sizeof...(rest) > 0)
        {
            return Insert(bindings, std::forward<Rest>(rest)...);
        }
    }
};
} // namespace mz::vk