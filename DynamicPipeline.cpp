
#include "DynamicPipeline.h"

namespace mz::vk
{
DynamicPipeline::DynamicPipeline(Device* Vk, VkExtent2D extent, View<u8> src)
    : Vk(Vk), Shader(MZShader::New(Vk, VK_SHADER_STAGE_FRAGMENT_BIT, src)), Layout(PipelineLayout::New(Vk, src)), Extent(extent)
{

    VertexShader* VS = Vk->GetGlobal<VertexShader>("GlobVS");

    if (0 == VS)
    {
        std::vector<u8> GlobalVSSPV = ReadSpirv(MZ_SHADER_PATH "/GlobVS.vert.spv");

        VS = Vk->RegisterGlobal<VertexShader>("GlobVS", Vk, GlobalVSSPV);
    }

#define FMT0 VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM
#define FMT1 FMT0, FMT0
#define FMT2 FMT1, FMT1
#define FMT  FMT2, FMT2

    constexpr static VkFormat FORMAT[] = {FMT, FMT};

    assert(Layout->RTCount <= sizeof(FORMAT) / sizeof(VkFormat));

    VkPipelineRenderingCreateInfo renderInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = Layout->RTCount,
        .pColorAttachmentFormats = FORMAT,
    };

    VkPipelineVertexInputStateCreateInfo inputLayout = VS->GetInputLayout();

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = VS->Module,
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

#define ATT0                                                                                                                        \
    {                                                                                                                               \
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT \
    }
#define ATT1 ATT0, ATT0
#define ATT2 ATT1, ATT1
#define ATT3 ATT2, ATT2
#define ATT  ATT3, ATT3

    constexpr static VkPipelineColorBlendAttachmentState attachments[] = {ATT, ATT};

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = Layout->RTCount,
        .pAttachments    = attachments,
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
        .pMultisampleState   = &multisampleState,
        .pColorBlendState    = &colorBlendState,
        .layout              = Layout->Handle,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handle));
}
} // namespace mz::vk