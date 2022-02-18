
#pragma once

#include "Layout.h"

struct MZShader : std::enable_shared_from_this<MZShader>, Uncopyable
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

        CHECKRE(Vk->CreateShaderModule(&info, 0, &Module));
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

struct DynamicPipeline : std::enable_shared_from_this<DynamicPipeline>, Uncopyable
{
    VulkanDevice* Vk;

    std::shared_ptr<MZShader>       Shader;
    std::shared_ptr<PipelineLayout> Layout;

    VkPipeline Handle;

    DynamicPipeline(VulkanDevice* Vk, VkExtent2D extent, const u32* src, u64 sz);

    ~DynamicPipeline()
    {
        Vk->DestroyPipeline(Handle, 0);
    }
};
