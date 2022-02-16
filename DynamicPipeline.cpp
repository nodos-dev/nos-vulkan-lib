
#include "DynamicPipeline.h"
#include "vulkan/vulkan_core.h"

DynamicPipeline::DynamicPipeline(std::shared_ptr<VulkanDevice> Vk, VkExtent2D extent, u32* src, u64 sz)
    : Vk(Vk), Shader(std::make_shared<MZShader>(Vk, VK_SHADER_STAGE_FRAGMENT_BIT, src, sz))
{
    if (GlobalVS.get() == nullptr)
    {
        std::string GlobalVSSPV = ReadToString(MZ_SHADER_PATH "/GlobVS.vert.spv", true);
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

    VkRect2D scissor = {.extent = extent};

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizationState = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_BACK_BIT,
        .frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth   = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo multisampleState = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState attachments = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &attachments,
    };

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    CHECKRE(Vk->CreatePipelineLayout(&layoutInfo, 0, &Layout));

    VkGraphicsPipelineCreateInfo info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &renderInfo,
        .stageCount          = 2,
        .pStages             = shaderStages,
        .pVertexInputState   = &inputLayout,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState   = &multisampleState,
        .pColorBlendState    = &colorBlendState,
        .layout              = Layout,
    };

    CHECKRE(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handle));
}