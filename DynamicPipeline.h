
#pragma once

#include "Layout.h"

namespace mz
{
struct MZShader : SharedFactory<MZShader>
{
    VulkanDevice*      Vk;
    VkShaderModule     Module;
    VkShaderStageFlags Stage;

    MZShader(VulkanDevice* Vk, VkShaderStageFlags stage, const u32* src, u64 sz)
        : Vk(Vk), Stage(stage)
    {
        VkShaderModuleCreateInfo info = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sz,
            .pCode    = src,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateShaderModule(&info, 0, &Module));
    }

    ~MZShader()
    {
        Vk->DestroyShaderModule(Module, 0);
    }
};

struct VertexShader : MZShader
{
    VkVertexInputBindingDescription                Binding;
    std::vector<VkVertexInputAttributeDescription> Attributes;

    VertexShader(VulkanDevice* Vk, const u32* src, u64 sz)
        : MZShader(Vk, VK_SHADER_STAGE_VERTEX_BIT, src, sz)
    {
        ReadInputLayout(src, sz, Binding, Attributes);
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

struct DynamicPipeline : SharedFactory<DynamicPipeline>
{
    VulkanDevice* Vk;

    std::shared_ptr<MZShader>       Shader;
    std::shared_ptr<PipelineLayout> Layout;

    VkPipeline Handle;

    VkExtent2D Extent;

    DynamicPipeline(VulkanDevice* Vk, VkExtent2D extent, const u32* src, u64 sz);

    ~DynamicPipeline()
    {
        Vk->DestroyPipeline(Handle, 0);
    }

    template <class... RT>
    requires((std::is_same_v<RT, VulkanImage*> || std::is_same_v<RT, std::shared_ptr<VulkanImage>>)&&...) void Bind(std::shared_ptr<CommandBuffer> Cmd, RT... Images)
    {
        assert(sizeof...(Images) == Layout->RTcount);

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
};
} // namespace mz