
#include "Shader.h"
#include "vulkan/vulkan_core.h"
#include <DynamicPipeline.h>

#include <Image.h>

namespace mz::vk
{

DynamicPipeline::~DynamicPipeline()
{
    Vk->DestroyPipeline(Handle, 0);
}

VertexShader* DynamicPipeline::GetVS() const
{
    if(!VS)
    {
        std::vector<u8> GlobalVSSPV = ReadSpirv(MZ_SHADER_PATH "/GlobVS.vert.spv");
        VS = Vk->RegisterGlobal<VertexShader>("GlobVS", Vk, GlobalVSSPV);
    }
    return VS;
}

void DynamicPipeline::ChangeTarget(rc<ImageView> Image)
{
    if(RenderTarget && (0 == memcmp(&RenderTarget->Src->Extent, &Image->Src->Extent, sizeof(VkExtent2D))) && RenderTarget->Format == Image->Format)
    {
        return;
    }
    if(Handle)
    {
        Vk->DestroyPipeline(Handle, 0);
    }
    CreateWithImage(Image);
}

void DynamicPipeline::CreateWithImage(rc<ImageView> Image)
{
    assert(1 == Layout->RTCount);
    
    RenderTarget = Image;
    
    VkExtent2D extent = Image->Src->Extent;

    VertexShader* VS = GetVS();
    
    VkPipelineRenderingCreateInfo renderInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = Layout->RTCount,
        .pColorAttachmentFormats = &Image->Format,
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
            .module = PS->Module,
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

    VkPipelineColorBlendAttachmentState attachment = {.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = Layout->RTCount,
        .pAttachments    = &attachment,
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

DynamicPipeline::DynamicPipeline(Device* Vk, View<u8> src, VkSampler sampler) 
    : DeviceChild(Vk), PS(Shader::New(Vk, VK_SHADER_STAGE_FRAGMENT_BIT, src)), Layout(PipelineLayout::New(Vk, src, sampler))
{
}

DynamicPipeline::DynamicPipeline(Device* Vk, VkExtent2D extent, View<u8> src, VkSampler sampler, std::vector<VkFormat> fmt)
    : DeviceChild(Vk), PS(Shader::New(Vk, VK_SHADER_STAGE_FRAGMENT_BIT, src)), Layout(PipelineLayout::New(Vk, src, sampler))
{

    for(u32 i = fmt.size(); i <= Layout->RTCount; ++i)
    {
        fmt.push_back(VK_FORMAT_R8G8B8A8_UNORM);
    }

    VkPipelineRenderingCreateInfo renderInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = Layout->RTCount,
        .pColorAttachmentFormats = fmt.data(),
    };
    
    VertexShader* VS = GetVS();
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
            .module = PS->Module,
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

void DynamicPipeline::BeginRendering(rc<CommandBuffer> Cmd, rc<ImageView> Image)
{
    if(!Image)
    {
        assert(RenderTarget);
    }
    else
    {
        ChangeTarget(Image);
    }

    VkRenderingAttachmentInfo Attachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = RenderTarget->Handle,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
    };

    RenderTarget->Src->Transition(Cmd, ImageState{
                                .StageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                .Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            });

    VkRenderingInfo renderInfo = {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {.extent = RenderTarget->Src->Extent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &Attachment,
    };

    Cmd->BeginRendering(&renderInfo);
    Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Handle);

    for (auto& set : DescriptorSets)
    {
        set->Bind(Cmd);
    }

    Cmd->AddDependency(shared_from_this());
}

bool DynamicPipeline::BindResources(rc<CommandBuffer> Cmd, std::unordered_map<std::string, Binding::Type> const& resources)
{
    std::map<u32, std::vector<Binding>> Bindings;

    for (auto& [name, res] : resources)
    {
        auto it = Layout->BindingsByName.find(name);
        if (it == Layout->BindingsByName.end())
        {
            return false;
        }
        Bindings[it->second.set].push_back(Binding(res, it->second.binding));
    }

    BindResources(Cmd, Bindings);

    return true;
}

void DynamicPipeline::BindResources(rc<CommandBuffer> Cmd, std::map<u32, std::vector<Binding>> const& bindings)
{

    Cmd->Callbacks.push_back([pipe = shared_from_this()]() { pipe->DescriptorSets.clear(); });
    
    for (auto& [idx, set] : bindings)
    {
        DescriptorSets.push_back(Layout->AllocateSet(idx)->Update(Cmd, set));
    }
}
} // namespace mz::vk