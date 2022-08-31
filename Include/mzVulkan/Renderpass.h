#pragma once

#include "Pipeline.h"

namespace mz::vk
{

struct Buffer;


struct VertexData
{
    rc<vk::Buffer> Buffer;
    u64 VertexOffset;
    u64 IndexOffset;
    u64 NumIndices;
    bool Wireframe = false;
};

struct mzVulkan_API Renderpass : SharedFactory<Renderpass>, DeviceChild
{
    VkFramebuffer FrameBuffer = 0;
    rc<ImageView> m_ImageView;
    rc<Pipeline> PL;
    
    rc<DescriptorPool> DescriptorPool;
    std::vector<rc<DescriptorSet>> DescriptorSets;
    std::map<u32, std::map<u32, vk::Binding>> Bindings;

    rc<Buffer> UniformBuffer;
    
    Renderpass(rc<Pipeline> PL);
    Renderpass(Device* Vk, View<u8> src);
    ~Renderpass();
    
    void Begin(rc<CommandBuffer> Cmd, rc<ImageView> Image, bool wireframe = false);
    void End(rc<CommandBuffer> Cmd);
    void Exec(rc<vk::CommandBuffer> Cmd, rc<vk::ImageView> Output, const VertexData* = 0);
    
    void Bind(std::string const& name, void* data, u32 size, rc<ImageView> (Import)(void*) = 0);

    void BindResources(std::map<u32, std::map<u32, Binding>> const &bindings);
    void BindResources(std::map<u32, std::vector<Binding>> const &bindings);
    bool BindResources(std::unordered_map<std::string, Binding::Type> const &resources);

    template <class... Args> requires(StringResourcePairPack<std::remove_cvref_t<Args>...>()) 
    bool BindResources(Args&&... args)
    {
        std::map<u32, std::vector<Binding>> bindings;
        if (!Insert(bindings, std::forward<Args>(args)...))
        {
            return false;
        }
        BindResources(bindings);
        return true;
    }

    template <class K, class V, class... Rest>
    bool Insert(std::map<u32, std::vector<Binding>>& bindings, K&& k, V&& v, Rest&&... rest)
    {
        auto it = PL->Layout->BindingsByName.find(k);
        if (it == PL->Layout->BindingsByName.end())
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
}