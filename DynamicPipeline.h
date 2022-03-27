
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
        assert(sizeof...(Images) == Layout->RTCount);

        (Images->Transition(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT), ...);

        VkRenderingAttachmentInfo colorAttachments[sizeof...(RT)] = {{
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = Images->View,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        }...};

        VkRenderingInfo renderInfo = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea           = {.extent = Extent},
            .layerCount           = 1,
            .colorAttachmentCount = sizeof...(RT),
            .pColorAttachments    = colorAttachments,
        };

        Cmd->BeginRendering(&renderInfo);
        Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Handle);
    }

    template <class T>
    void PushConstants(rc<CommandBuffer> Cmd, T const& data)
    {
        Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
    }

    template <TypeClassResource Resource>
    bool BindResource(rc<CommandBuffer> Cmd, std::string name, Resource res)
    {
        if (auto it = Layout->BindingsByName.find(name); it != Layout->BindingsByName.end())
        {
            glm::uvec2 idx = it->second.xy;

            rc<DescriptorSet> set = Layout->AllocateSet(idx.x)->UpdateWith(Binding(res, idx.y))->Bind(Cmd);

            Cmd->Callbacks.push_back([set]() {});
            return true;
        }
        return false;
    }

    template <TypeClassResource... Res, TypeClassString... Name>
    bool BindResources(rc<CommandBuffer> Cmd, std::pair<Res, Name>... res)
    {
        std::map<u32, std::vector<Binding>> Bindings;

        auto Inserter = [&Bindings, this](auto res, auto& name) {
            if (auto it = Layout->BindingsByName.find(name); it != Layout->BindingsByName.end())
            {
                Bindings[it->second.x].push_back(Binding(res, it->second.y));
                return true;
            }
            return false;
        };

        if (!(Inserter(res.first, res.second) && ...))
        {
            return false;
        }

        std::vector<rc<DescriptorSet>> sets;

        for (auto& [idx, set] : Bindings)
        {
            sets.push_back(Layout->AllocateSet(idx)->UpdateWith(set)->Bind(Cmd));
        }

        Cmd->Callbacks.push_back([sets]() {});

        return true;
    }
};
} // namespace mz::vk