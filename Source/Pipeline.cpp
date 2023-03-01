// Copyright MediaZ AS. All Rights Reserved.


#include "mzVulkan/Pipeline.h"
#include "mzVulkan/Common.h"
#include "mzVulkan/Image.h"
#include "GlobVS.vert.spv.dat"
#include "vulkan/vulkan_core.h"

namespace mz::vk
{

Pipeline::Pipeline(Device* Vk, std::vector<u8> const& src)
    : Pipeline(Vk, Shader::New(Vk, src))
{
}

Pipeline::Pipeline(Device* Vk, rc<Shader> SS) 
    : DeviceChild(Vk), MainShader(SS), Layout(PipelineLayout::New(Vk, SS->Layout))
{
}

ComputePipeline::ComputePipeline(Device* Vk, std::vector<u8> const& src)
    : ComputePipeline(Vk, Shader::New(Vk, src))
{

}

ComputePipeline::ComputePipeline(Device* Vk, rc<Shader> CS)
    : Pipeline(Vk, CS)
{
    VkComputePipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = 0,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = MainShader->Module,
            .pName = "main",
        },
        .layout = Layout->Handle,
    };
    MZVK_ASSERT(Vk->CreateComputePipelines(0, 1, &info, 0, &Handle));
}


GraphicsPipeline::~GraphicsPipeline()
{
    for(auto [_, handl]: Handles)
    {
        if(handl.pl)
        {
            Vk->DestroyPipeline(handl.pl, 0);
        }
        if(handl.wpl)
        {
            Vk->DestroyPipeline(handl.wpl, 0);
        }
        if(handl.rp)
        {
            Vk->DestroyRenderPass(handl.rp, 0);
        }
    }
}

GraphicsPipeline::GraphicsPipeline(Device* Vk, rc<Shader> PS, rc<Shader> VS, bool blend) 
    : Pipeline(Vk, PS), VS(VS), EnableBlending(blend)
{
    Layout = PipelineLayout::New(Vk, PS->Layout.Merge(GetVS()->Layout));
}

GraphicsPipeline::GraphicsPipeline(Device* Vk, std::vector<u8> const& src, bool blend) :
    GraphicsPipeline(Vk, Shader::New(Vk, src), 0, blend)
{

}

rc<Shader> GraphicsPipeline::GetVS()
{
    if (!VS)
    {
        if (!GlobVS)
        {
            std::vector<u8> GlobalVSSPV(GlobVS_vert_spv, GlobVS_vert_spv + (sizeof(GlobVS_vert_spv) & ~3));
            GlobVS = *Vk->RegisterGlobal<rc<Shader>>("GlobVS", MakeShared<Shader>(Vk, GlobalVSSPV));
        }
        VS = GlobVS;
    }
    return VS;
}

void GraphicsPipeline::Recreate(VkFormat fmt)
{
    if(Handles[fmt].pl)
    {
        return;
    }

    VkPipelineRenderingCreateInfo renderInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = Layout->RTCount,
        .pColorAttachmentFormats = &fmt,
        // .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
    };

    VkPipelineVertexInputStateCreateInfo inputLayout = {};
    VS->GetInputLayout(&inputLayout);

    VkPipelineShaderStageCreateInfo shaderStages[2] = {{
                                                           .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                           .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                           .module = VS->Module,
                                                           .pName = "main",
                                                       },
                                                       {
                                                           .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                           .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .module = MainShader->Module,
                                                           .pName = "main",
                                                       }};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineRasterizationStateCreateInfo rasterizationState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        // .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo multisampleState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState attachment = {
        .blendEnable = EnableBlending,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .alphaBlendOp = VK_BLEND_OP_MAX,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = Layout->RTCount,
        .pAttachments = &attachment,
    };
    
    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = fmt;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        MZVK_ASSERT(Vk->CreateRenderPass(&renderPassInfo, nullptr, &Handles[fmt].rp));
    }

    VkDynamicState states[] = { VK_DYNAMIC_STATE_VIEWPORT, 
                                VK_DYNAMIC_STATE_SCISSOR, 
                                VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                                VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
                                VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                            };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = sizeof(states) / sizeof(states[0]),
        .pDynamicStates = states,
    };

    VkPipelineViewportStateCreateInfo  viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineDepthStencilStateCreateInfo depthState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = 0,
        .depthWriteEnable      = 0,
        .depthCompareOp        = VK_COMPARE_OP_NEVER,
        .depthBoundsTestEnable = 0,
    };

    VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &inputLayout,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = Layout->Handle,
    };
    
    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        info.renderPass = Handles[fmt].rp;
        info.pNext = 0;
    }

    MZVK_ASSERT(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handles[fmt].pl));
    rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
    MZVK_ASSERT(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handles[fmt].wpl));
}


} // namespace mz::vk