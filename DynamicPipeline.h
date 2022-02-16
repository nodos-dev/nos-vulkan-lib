
#pragma once

#include "Device.h"

#include "Buffer.h"

void ReadInputLayout(const u32* src, u64 sz, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes);

struct MZShader : std::enable_shared_from_this<MZShader>
{
    std::shared_ptr<VulkanDevice> Vk;
    VkShaderModule                Module;
    VkShaderStageFlags            Stage;

    MZShader()
        : Vk(0), Module(0), Stage(0)
    {
    }

    MZShader(std::shared_ptr<VulkanDevice> Vk, VkShaderStageFlags stage, const u32* src, u64 sz)
        : Vk(Vk), Stage(stage)
    {
        VkShaderModuleCreateInfo info = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = sz,
            .pCode    = src,
        };

        CHECKRE(Vk->CreateShaderModule(&info, 0, &Module));
    }
};

struct VertexShader : MZShader
{

    VkVertexInputBindingDescription                Binding;
    std::vector<VkVertexInputAttributeDescription> Attributes;

    VertexShader()
        : MZShader(), Binding(), Attributes()
    {
    }

    VertexShader(std::shared_ptr<VulkanDevice> VkIN, const u32* src, u64 sz)
        : MZShader(std::move(VkIN), VK_SHADER_STAGE_VERTEX_BIT, src, sz)
    {
        ReadInputLayout(src, sz, Binding, Attributes);
    }

    VkPipelineVertexInputStateCreateInfo GetInputLayout()
    {
        VkPipelineVertexInputStateCreateInfo InputLayout = {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &Binding,
            .vertexAttributeDescriptionCount = (u32)Attributes.size(),
            .pVertexAttributeDescriptions    = Attributes.data(),
        };
        return InputLayout;
    }
};

struct DynamicPipeline : std::enable_shared_from_this<DynamicPipeline>
{
    std::shared_ptr<VulkanDevice> Vk;

    inline static std::shared_ptr<VertexShader> GlobalVS;

    std::shared_ptr<MZShader> Shader;

    VkPipeline       Handle;
    VkPipelineLayout Layout;

    DynamicPipeline(std::shared_ptr<VulkanDevice> Vk, VkExtent2D extent, u32* src, u64 sz);
};
