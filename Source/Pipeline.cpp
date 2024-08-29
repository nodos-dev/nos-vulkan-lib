// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


#include "nosVulkan/Pipeline.h"
#include "nosVulkan/Common.h"
#include "nosVulkan/Image.h"
#include "GlobVS.h"
#include "vulkan/vulkan_core.h"

namespace nos::vk
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
    NOSVK_ASSERT(Vk->CreateComputePipelines(Vk->PipelineCache, 1, &info, 0, &Handle));
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


GraphicsPipeline::GraphicsPipeline(Device* Vk, rc<Shader> PS, rc<Shader> VS, BlendMode blend, u32 ms) 
    : Pipeline(Vk, PS), VS(VS), Blend(blend), MS(std::max(ms, 1u))
{
    Layout = PipelineLayout::New(Vk, PS->Layout.Merge(GetVS()->Layout));
}

GraphicsPipeline::GraphicsPipeline(Device* Vk, std::vector<u8> const& src, BlendMode blend, u32 ms) :
    GraphicsPipeline(Vk, Shader::New(Vk, src), 0, blend, ms)
{

}

rc<Shader> GraphicsPipeline::GetVS()
{
    if (!VS)
    {
        if (!Vk->Globals.contains("GlobVS"))
            Vk->RegisterGlobal<rc<Shader>>("GlobVS", Vk, std::vector<u8>(GlobVS_vert_spv, GlobVS_vert_spv + (sizeof(GlobVS_vert_spv) & ~3)));
        VS = Vk->GetGlobal<rc<Shader>>("GlobVS");
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
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
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
        .rasterizationSamples = VkSampleCountFlagBits(MS),
    };

    VkPipelineColorBlendAttachmentState attachment = {
        .blendEnable = Blend.Enable,
        .srcColorBlendFactor = (VkBlendFactor)Blend.SrcColorFactor,
        .dstColorBlendFactor = (VkBlendFactor)Blend.DstColorFactor,
        .colorBlendOp = (VkBlendOp)Blend.ColorOp,
		.srcAlphaBlendFactor = (VkBlendFactor)Blend.SrcAlphaFactor,
		.dstAlphaBlendFactor = (VkBlendFactor)Blend.DstAlphaFactor,
		.alphaBlendOp = (VkBlendOp)Blend.AlphaOp,
		.colorWriteMask = (VkColorComponentFlags)Blend.ColorMask,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = Layout->RTCount,
        .pAttachments = &attachment,
    };
    
    if (!Vk->Features.dynamicRendering)
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

        NOSVK_ASSERT(Vk->CreateRenderPass(&renderPassInfo, nullptr, &Handles[fmt].rp));
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
    
    if (!Vk->Features.dynamicRendering)
    {
        info.renderPass = Handles[fmt].rp;
        info.pNext = 0;
    }
    NOSVK_ASSERT(Vk->CreateGraphicsPipelines(Vk->PipelineCache, 1, &info, 0, &Handles[fmt].pl));
    rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
    NOSVK_ASSERT(Vk->CreateGraphicsPipelines(Vk->PipelineCache, 1, &info, 0, &Handles[fmt].wpl));
}


} // namespace nos::vk