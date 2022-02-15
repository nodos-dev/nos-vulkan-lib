#pragma once

#include "mzVkCommon.h"

struct RenderPass
{
    VkRenderPass handle;

    void create(VkDevice dev, VkFormat format)
    {
        VkAttachmentDescription attachments[] = {
            {
                .format        = format,
                .samples       = VK_SAMPLE_COUNT_1_BIT,
                .loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            {
                .format        = format,
                .samples       = VK_SAMPLE_COUNT_1_BIT,
                .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        };

        VkAttachmentReference ref[] = {
            {
                .attachment = 0,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            {
                .attachment = 0,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .attachment = 1,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        };

        VkSubpassDescription subpass[] = {
            {
                .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &ref[0],
            },
            {
                .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 1,
                .pInputAttachments    = &ref[1],
                .colorAttachmentCount = 1,
                .pColorAttachments    = &ref[2],
            },
        };

        VkSubpassDependency deps[] = {
            {
                .srcSubpass    = VK_SUBPASS_EXTERNAL,
                .dstSubpass    = 0,
                .srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
            {
                .srcSubpass    = 0,
                .dstSubpass    = 1,
                .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
            },
        };

        VkRenderPassCreateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = sizeof attachments / sizeof attachments[0],
            .pAttachments    = attachments,
            .subpassCount    = sizeof subpass / sizeof subpass[0],
            .pSubpasses      = subpass,
            .dependencyCount = sizeof deps / sizeof deps[0],
            .pDependencies   = deps,
        };

        CHECKRE(vkCreateRenderPass(dev, &info, 0, &handle));
    }


    void free(VkDevice dev)
    {
        vkDestroyRenderPass(dev, handle, 0);
    }
};
