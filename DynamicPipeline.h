
#pragma once

#include "Layout.h"

namespace mz::vk
{
struct mzVulkan_API MZShader : SharedFactory<MZShader>
{
    Device* Vk;
    VkShaderModule Module;
    VkShaderStageFlags Stage;

    MZShader(Device* Vk, VkShaderStageFlags stage, View<u8> src)
        : Vk(Vk), Stage(stage)
    {
        VkShaderModuleCreateInfo info = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = src.size(),
            .pCode    = (u32*)src.data(),
        };
        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateShaderModule(&info, 0, &Module));
    }

    ~MZShader()
    {
        Vk->DestroyShaderModule(Module, 0);
    }
};

struct mzVulkan_API VertexShader : MZShader
{
    VkVertexInputBindingDescription Binding;
    std::vector<VkVertexInputAttributeDescription> Attributes;

    VertexShader(Device* Vk, View<u8> src)
        : MZShader(Vk, VK_SHADER_STAGE_VERTEX_BIT, src)
    {
        ReadInputLayout(src, Binding, Attributes);
    }

    VkPipelineVertexInputStateCreateInfo GetInputLayout()
    {
        VkPipelineVertexInputStateCreateInfo InputLayout = {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexAttributeDescriptionCount = (u32)Attributes.size(),
            .pVertexAttributeDescriptions    = Attributes.data(),
        };

        if (!Attributes.empty())
        {
            InputLayout.vertexBindingDescriptionCount = 1;
            InputLayout.pVertexBindingDescriptions    = &Binding;
        }

        return InputLayout;
    }
};

struct mzVulkan_API DynamicPipeline : SharedFactory<DynamicPipeline>
{
    Device* Vk;

    std::shared_ptr<MZShader> Shader;
    std::shared_ptr<PipelineLayout> Layout;

    VkPipeline Handle;

    VkExtent2D Extent;

    DynamicPipeline(Device* Vk, VkExtent2D extent, View<u8> src);

    ~DynamicPipeline()
    {
        Vk->DestroyPipeline(Handle, 0);
    }

    void BeginWithRTs(std::shared_ptr<CommandBuffer> Cmd, View<std::shared_ptr<Image>> Images)
    {
        assert(Images.size() == Layout->RTCount);

        std::vector<VkRenderingAttachmentInfo> Attachments;
        Attachments.reserve(Images.size());

        for (auto img : Images)
        {
            img->Transition(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            Attachments.push_back(VkRenderingAttachmentInfo{
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = img->View,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
            });
        }

        VkRenderingInfo renderInfo = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea           = {.extent = Extent},
            .layerCount           = 1,
            .colorAttachmentCount = (u32)Attachments.size(),
            .pColorAttachments    = Attachments.data(),
        };

        Cmd->BeginRendering(&renderInfo);
        Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Handle);
    }

    template <std::same_as<std::shared_ptr<vk::Image>>... RT>
    void BeginWithRTs(std::shared_ptr<CommandBuffer> Cmd, RT... Images)
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
    void PushConstants(std::shared_ptr<CommandBuffer> Cmd, T const& data)
    {
        Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
    }

    template <TypeClassResource Resource>
    bool BindResource(std::shared_ptr<CommandBuffer> Cmd, std::string name, Resource res)
    {
        if (auto it = Layout->BindingsByName.find(name); it != Layout->BindingsByName.end())
        {
            glm::uvec2 idx = it->second.xy;

            std::shared_ptr<DescriptorSet> set = Layout->AllocateSet(idx.x)->UpdateWith(Binding(res, idx.y))->Bind(Cmd);

            Cmd->Callbacks.push_back([set]() {});
            return true;
        }
        return false;
    }

    template <TypeClassResource... Res, TypeClassString... Name>
    bool BindResources(std::shared_ptr<CommandBuffer> Cmd, std::pair<Res, Name>... res)
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

        std::vector<std::shared_ptr<DescriptorSet>> sets;

        for (auto& [idx, set] : Bindings)
        {
            sets.push_back(Layout->AllocateSet(idx)->UpdateWith(set)->Bind(Cmd));
        }

        Cmd->Callbacks.push_back([sets]() {});

        return true;
    }
};
} // namespace mz::vk