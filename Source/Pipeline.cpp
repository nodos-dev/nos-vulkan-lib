
#include "Shader.h"
#include "vulkan/vulkan_core.h"
#include <Pipeline.h>

#include <Image.h>

namespace mz::vk
{

Pipeline::~Pipeline()
{
    Vk->DestroyPipeline(Handle, 0);
}

VertexShader *Pipeline::GetVS() const
{
    if (!VS)
    {
        std::vector<u8> GlobalVSSPV = ReadSpirv(MZ_SHADER_PATH "/GlobVS.vert.spv");
        VS = Vk->RegisterGlobal<VertexShader>("GlobVS", Vk, GlobalVSSPV);
    }
    return VS;
}

void Pipeline::Recreate(VkFormat fmt)
{
    if (Handle)
    {
        Vk->DestroyPipeline(Handle, 0);
    }

    VertexShader *VS = GetVS();

    VkPipelineRenderingCreateInfo renderInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = Layout->RTCount,
        .pColorAttachmentFormats = &fmt,
    };

    VkPipelineVertexInputStateCreateInfo inputLayout = VS->GetInputLayout();

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
        .cullMode = VK_CULL_MODE_BACK_BIT,
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
        colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
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

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateRenderPass(&renderPassInfo, nullptr, &RenderPass));
    }

    VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT};

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = sizeof(states) / sizeof(states[0]),
        .pDynamicStates = states,
    };

    VkGraphicsPipelineCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &inputLayout,
        .pInputAssemblyState = &inputAssembly,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = Layout->Handle,
    };

    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        info.renderPass = RenderPass;
        info.pNext = 0;
    }
    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateGraphicsPipelines(0, 1, &info, 0, &Handle));
}

Pipeline::Pipeline(Device *Vk, View<u8> src)
    : DeviceChild(Vk), PS(Shader::New(Vk, VK_SHADER_STAGE_FRAGMENT_BIT, src)), Layout(PipelineLayout::New(Vk, src))
{
}


void Pipeline::BeginRendering(rc<CommandBuffer> Cmd, rc<ImageView> Image)
{
    assert(Image);

    if(Format != Image->GetEffectiveFormat())
    {
        Recreate(Image->GetEffectiveFormat());
    }

    Image->Src->Transition(Cmd, ImageState{
                                           .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       });

    for (auto &set : DescriptorSets)
    {
        set->Bind(Cmd);
    }

    VkExtent2D extent = Image->Src->GetEffectiveExtent();

    VkViewport viewport = {
        .width = (f32)extent.width,
        .height = (f32)extent.height,
        .maxDepth = 1.f,
    };

    Cmd->SetViewport(0, 1, &viewport);

    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = RenderPass,
            .attachmentCount = 1,
            .pAttachments = &Image->Handle,
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateFramebuffer(&framebufferInfo, nullptr, &FrameBuffer));

        VkRenderPassBeginInfo renderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = RenderPass,
            .framebuffer = FrameBuffer,
            .renderArea = {{0, 0}, extent},
        };

        Cmd->BeginRenderPass(&renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    else
    {
        VkRenderingAttachmentInfo Attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = Image->Handle,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        VkRenderingInfo renderInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = extent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &Attachment,
        };

        Cmd->BeginRendering(&renderInfo);
    }

    Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Handle);

    Cmd->AddDependency(shared_from_this());
}

void Pipeline::EndRendering(rc<CommandBuffer> Cmd)
{
    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        Cmd->EndRenderPass();
    }
    else
    {
        Cmd->EndRendering();
    }
}

bool Pipeline::BindResources(std::unordered_map<std::string, Binding::Type> const &resources)
{
    std::map<u32, std::map<u32, Binding>> Bindings;

    for (auto &[name, res] : resources)
    {
        auto it = Layout->BindingsByName.find(name);
        if (it == Layout->BindingsByName.end())
        {
            return false;
        }
        Bindings[it->second.set][it->second.binding] = Binding(res, it->second.binding);
    }

    BindResources(Bindings);

    return true;
}

void Pipeline::BindResources(std::map<u32, std::vector<Binding>> const &bindings)
{
    DescriptorSets.clear();
    for (auto &[idx, set] : bindings)
    {
        DescriptorSets.push_back(Layout->AllocateSet(idx)->Update(set));
    }
}

void Pipeline::BindResources(std::map<u32, std::map<u32, Binding>> const &bindings)
{
    DescriptorSets.clear();
    for (auto &[idx, set] : bindings)
    {
        DescriptorSets.push_back(Layout->AllocateSet(idx)->Update(set));
    }
}
} // namespace mz::vk