
#include "mzVulkan/Pipeline.h"
#include "mzVulkan/Image.h"
#include "GlobVS.vert.spv.dat"

namespace mz::vk
{

Pipeline::~Pipeline()
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

Pipeline::Pipeline(Device* Vk, View<u8> src) :
    Pipeline(Vk, Shader::New(Vk, src))
{

}

rc<Shader> Pipeline::GetVS()
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

void Pipeline::Recreate(VkFormat fmt)
{
    if(Handles[fmt].pl)
    {
        return;
    }


    VkPipelineRenderingCreateInfo renderInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = Layout->RTCount,
        .pColorAttachmentFormats = &fmt,
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
                                                           .module = PS->Module,
                                                           .pName = "main",
                                                       }};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineRasterizationStateCreateInfo rasterizationState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo multisampleState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState attachment = {.colorWriteMask =
                                                          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

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

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateRenderPass(&renderPassInfo, nullptr, &Handles[fmt].rp));
    }

    VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

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
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = Layout->Handle,
    };

    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        info.renderPass = Handles[fmt].rp;
        info.pNext = 0;
    }

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handles[fmt].pl));
    rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handles[fmt].wpl));
}

Pipeline::Pipeline(Device* Vk, rc<Shader> PS, rc<Shader> VS) 
    : DeviceChild(Vk), PS(PS), VS(VS), Layout(PipelineLayout::New(Vk, PS->Layout.Merge(GetVS()->Layout)))
{
}

} // namespace mz::vk