
#pragma once

#include "Device.h"

#include "Buffer.h"
#include "mzCommon.h"
#include "vulkan/vulkan_core.h"
#include <memory>
#include <vector>

enum MZShaderKind
{
    MZ_SHADER_VERTEX,
    MZ_SHADER_PIXEL,
    MZ_SHADER_COMPUTE,
};

enum MZOptLevel
{
    MZ_OPT_LEVEL_NONE,
    MZ_OPT_LEVEL_SIZE,
    MZ_OPT_LEVEL_PERF,
};

std::vector<uint32_t> CompileFile(const std::string&                              source_name,
                                  MZShaderKind                                    kind,
                                  const std::string&                              source,
                                  std::string&                                    err,
                                  enum MZOptLevel                                 opt,
                                  VkVertexInputBindingDescription*                binding    = 0,
                                  std::vector<VkVertexInputAttributeDescription>* attributes = 0);

struct MZShader : std::enable_shared_from_this<MZShader>
{
    std::shared_ptr<VulkanDevice> Vk;
    VkShaderModule                Module;
    MZShaderKind                  Kind;
    std::string                   Name;

    MZShader()
        : Vk(0), Module(0), Kind(MZ_SHADER_VERTEX)
    {
    }

    MZShader(std::shared_ptr<VulkanDevice> Vk, MZShaderKind kind, std::string shaderNameIN, std::string const& source, VkVertexInputBindingDescription* binding = 0, std::vector<VkVertexInputAttributeDescription>* attributes = 0)
        : Vk(Vk), Kind(kind), Name(std::move(shaderNameIN))
    {
        std::string err;

        std::vector<u32> bin = CompileFile(Name, kind, source, err,
#ifdef _DEBUG
                                           MZ_OPT_LEVEL_NONE
#else
                                           MZ_OPT_LEVEL_PERF
#endif
        );

        VkShaderModuleCreateInfo info = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = bin.size() * 4,
            .pCode    = bin.data(),
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

    VertexShader(std::shared_ptr<VulkanDevice> VkIN, std::string const& fileName)
        : MZShader(std::move(VkIN), MZ_SHADER_VERTEX, fileName, ReadToString(fileName), &Binding, &Attributes)
    {
    }

    VertexShader(std::shared_ptr<VulkanDevice> VkIN, std::string shaderName, std::string const& source)
        : MZShader(std::move(VkIN), MZ_SHADER_VERTEX, std::move(shaderName), source, &Binding, &Attributes)
    {
    }
};

struct DynamicPipeline : std::enable_shared_from_this<DynamicPipeline>
{
    std::shared_ptr<VulkanDevice> Vk;

    inline static std::shared_ptr<VertexShader> GlobalVS;

    std::shared_ptr<MZShader> Shader;

    VkPipeline Handle;

    DynamicPipeline(std::shared_ptr<VulkanDevice> Vk, VkExtent2D extent, std::string nameIN, std::string const& source)
        : Vk(Vk), Shader(std::make_shared<MZShader>(Vk, MZ_SHADER_PIXEL, std::move(nameIN), source))
    {
        if (GlobalVS.get() != nullptr)
        {
            GlobalVS = std::make_shared<VertexShader>(Vk, "GlobalVS",
#include "GlobVS.vert"
            );
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
