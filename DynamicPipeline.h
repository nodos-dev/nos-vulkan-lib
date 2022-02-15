
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

    VkPipeline Handle;

    DynamicPipeline(std::shared_ptr<VulkanDevice> Vk, VkExtent2D extent, u32* src, u64 sz)
        : Vk(Vk), Shader(std::make_shared<MZShader>(Vk, VK_SHADER_STAGE_FRAGMENT_BIT, src, sz))
    {
        if (GlobalVS.get() != nullptr)
        {
            std::string GlobalVSSPV = ReadToString(MZ_SHADER_PATH "/GlobalVS.spv", true);
            GlobalVS                = std::make_shared<VertexShader>(Vk, (u32*)GlobalVSSPV.data(), GlobalVSSPV.size());
        }

        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

        VkPipelineRenderingCreateInfo renderInfo = {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &format,
        };

        VkPipelineVertexInputStateCreateInfo inputLayout = GlobalVS->GetInputLayout();

        VkPipelineShaderStageCreateInfo shaderStages[2] = {
            {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                .module = GlobalVS->Module,
                .pName  = "main",
            },
            {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = Shader->Module,
                .pName  = "main",
            }};

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        VkViewport viewport = {
            .width    = (f32)extent.width,
            .height   = (f32)extent.height,
            .maxDepth = 1.f,
        };

        VkPipelineViewportStateCreateInfo viewportState = {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports    = &viewport,
        };

        VkPipelineRasterizationStateCreateInfo rasterizationState = {
            .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode    = VK_CULL_MODE_BACK_BIT,
            .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth   = 1.f,
        };

        VkGraphicsPipelineCreateInfo info = {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = &renderInfo,
            .stageCount          = 2,
            .pStages             = shaderStages,
            .pVertexInputState   = &inputLayout,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizationState,
        };

        CHECKRE(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handle));
    }
};
