
#pragma once

#include "Layout.h"

namespace mz::vk
{
struct MZVULKAN_API MZShader : SharedFactory<MZShader>
{
    Device*            Vk;
    VkShaderModule     Module;
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

struct MZVULKAN_API VertexShader : MZShader
{
    VkVertexInputBindingDescription                Binding;
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

struct MZVULKAN_API DynamicPipeline : SharedFactory<DynamicPipeline>
{
    Device* Vk;

    std::shared_ptr<MZShader>       Shader;
    std::shared_ptr<PipelineLayout> Layout;

    VkPipeline Handle;

    VkExtent2D Extent;

    DynamicPipeline(Device* Vk, VkExtent2D extent, View<u8> src);

    ~DynamicPipeline()
    {
        Vk->DestroyPipeline(Handle, 0);
    }

    template <TypeClassImage... RT>
    void BeginWithRTs(std::shared_ptr<CommandBuffer> Cmd, RT... Images)
    {
        assert(sizeof...(Images) == Layout->RTCount);

        (Images->Transition(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT), ...);

        VkRenderingAttachmentInfo colorAttachments[sizeof...(Images)] = {{
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
            .colorAttachmentCount = sizeof...(Images),
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
};
} // namespace mz::vk