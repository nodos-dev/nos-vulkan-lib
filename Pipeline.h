#pragma once

#include "Shader.h"

template <class... Args>
u64 hash_fold(Args&&... args)
{
    struct Xformer
    {
        u64 hash;

        Xformer(u64 seed)
            : hash(seed)
        {
        }

        Xformer& operator+(Xformer r)
        {
            hash ^= (hash << 6) + (hash >> 2) + 0x9e3779b9 + r.hash;
            return *this;
        }
    };

    return ((Xformer(args->Hash()) + ...)).hash;
}

struct GraphicsPipelineState
{
    VkCullModeFlags     culling     = VK_CULL_MODE_NONE;
    VkPolygonMode       polygon     = VK_POLYGON_MODE_FILL;
    VkPrimitiveTopology topology    = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool                depth_test  = true;
    bool                depth_write = true;
    bool                alpha       = true;
    f32                 line_width  = 1;
    VkExtent2D          extent      = {};
    VkRenderPass        renderpass  = 0;
    u32                 subpass     = 0;

    std::vector<VkVertexInputBindingDescription>   bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

struct Pipeline
{
    VkDevice             dev;
    VkPipeline           handle;
    PipelineLayout       layout;
    std::vector<Shaderx> shaders;
    VkPipelineBindPoint  point;

    std::unordered_map<size_t, VkDescriptorSet> sets;

    void bind(VkCommandBuffer cmd)
    {
        return vkCmdBindPipeline(cmd, point, handle);
    }

    template <class... Args>
    void bind_set(VkCommandBuffer cmd, u32 idx, Args&&... args)
    {
        u64 hash = hash_fold(args...);

        VkDescriptorSet* set = &sets[hash];

        if (*set)
        {
            return vkCmdBindDescriptorSets(cmd, point, layout.handle, idx, 1, set, 0, 0);
        }

        *set = AllocateSet(idx);

        auto* binding = layout.descriptors[idx].bindings.data();

        (((args->bind_to_set(dev, binding->type, binding->binding, *set)), binding++), ...);

        return vkCmdBindDescriptorSets(cmd, point, layout.handle, idx, 1, set, 0, 0);
    }

    VkResult create(VkDevice dev, std::vector<std::vector<u32>> bins, GraphicsPipelineState* gfx_state = 0)
    {
        this->dev = dev;
        shaders.clear();
        shaders.reserve(bins.size());

        for (auto& bin : bins)
        {
            Shaderx s;
            s.Create(dev, &layout, bin);
            shaders.emplace_back(s);
        }

        layout.Create(dev);

        if (shaders[0].stage == VK_SHADER_STAGE_COMPUTE_BIT)
        {
            return create_compute(dev);
        }

        return create_graphics(dev, gfx_state);
    }

    VkResult create_graphics(VkDevice dev, GraphicsPipelineState* state)
    {
        point = VK_PIPELINE_BIND_POINT_GRAPHICS;

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        stages.reserve(shaders.size());

        for (auto& shader : shaders)
        {
            stages.push_back(VkPipelineShaderStageCreateInfo{
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = shader.stage,
                .module = shader.mod,
                .pName  = shader.entry.c_str(),
            });
        }

        VkPipelineVertexInputStateCreateInfo VertexInputState = {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = (u32)state->bindings.size(),
            .pVertexBindingDescriptions      = state->bindings.data(),
            .vertexAttributeDescriptionCount = (u32)state->attributes.size(),
            .pVertexAttributeDescriptions    = state->attributes.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = {
            .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = state->topology,
        };

        VkViewport Viewports = {
            .width    = (f32)state->extent.width,
            .height   = (f32)state->extent.height,
            .maxDepth = 1.f,
        };

        VkRect2D                          Scissors      = {.extent = state->extent};
        VkPipelineViewportStateCreateInfo ViewportState = {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports    = &Viewports,
            .scissorCount  = 1,
            .pScissors     = &Scissors,
        };

        VkPipelineMultisampleStateCreateInfo MultisampleState = {
            .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineDepthStencilStateCreateInfo DepthStencilState = {
            .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable  = state->depth_test,
            .depthWriteEnable = state->depth_write,
            .depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL,
        };

        VkPipelineColorBlendAttachmentState Attachments = {
            .blendEnable         = state->alpha,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo ColorBlendState = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments    = &Attachments,
        };

        VkPipelineDynamicStateCreateInfo DynamicState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        };

        VkPipelineRasterizationStateCreateInfo RasterizationState = {
            .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = state->polygon,
            .cullMode    = state->culling,
            .lineWidth   = state->line_width};

        VkGraphicsPipelineCreateInfo info = {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount          = (u32)stages.size(),
            .pStages             = stages.data(),
            .pVertexInputState   = &VertexInputState,
            .pInputAssemblyState = &InputAssemblyState,
            .pViewportState      = &ViewportState,
            .pRasterizationState = &RasterizationState,
            .pMultisampleState   = &MultisampleState,
            .pDepthStencilState  = &DepthStencilState,
            .pColorBlendState    = &ColorBlendState,
            .pDynamicState       = &DynamicState,
            .layout              = layout.handle,
            .renderPass          = state->renderpass,
            .subpass             = state->subpass,
        };

        return vkCreateGraphicsPipelines(dev, 0, 1, &info, 0, &handle);
    }

    VkResult create_compute(VkDevice dev)
    {
        point = VK_PIPELINE_BIND_POINT_COMPUTE;

        VkComputePipelineCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {
                .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shaders[0].mod,
                .pName  = shaders[0].entry.data(),
            },
            .layout = layout.handle,
        };

        return vkCreateComputePipelines(dev, 0, 1, &info, 0, &handle);
    }

    VkDescriptorSet AllocateSet(u32 idx)
    {
        VkDescriptorSet set;
        layout.AllocateSet(dev, idx, &set);
        return set;
    }
};
