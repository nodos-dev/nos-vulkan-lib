#pragma once

#include "Buffer.h"
#include "Command.h"
#include "vulkan/vulkan_core.h"
#include <memory>

struct ImageCreateInfo
{
    VkExtent2D        Extent;
    VkImageLayout     FinalLayout;
    VkFormat          Format;
    VkImageUsageFlags Usage;
    u32               MipLevels;
};

struct VulkanImage : std::enable_shared_from_this<VulkanImage>
{
    VulkanDevice* Vk;

    VkImage Handle;

    VkExtent2D        Extent;
    VkFormat          Format;
    VkImageUsageFlags Usage;
    u32               MipLevels;

    Allocation Allocation;

    VkImageLayout FinalLayout;

    union {
        DescriptorResourceInfo ResourceInfo;
        struct
        {
            VkSampler     Sampler;
            VkImageView   View;
            VkImageLayout Layout;
        };
    };

    DescriptorResourceInfo GetDescriptorInfo() const
    {
        return ResourceInfo;
    }

    ~VulkanImage()
    {
        Vk->DestroyImage(Handle, 0);
        Vk->DestroyImageView(View, 0);
        Vk->DestroySampler(Sampler, 0);
    }

    void Transition(
        std::shared_ptr<CommandBuffer> cmd,
        VkImageLayout                  TargetLayout)
    {

        if (Layout == TargetLayout)
        {
            return;
        }

        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout        = Layout,
            .newLayout        = TargetLayout,
            .image            = Handle,
            .subresourceRange = {
                .aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount   = 1,
                .layerCount   = 1,
            },
        };

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch (Layout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            imageMemoryBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (TargetLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (imageMemoryBarrier.srcAccessMask == 0)
            {
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Put barrier inside setup command buffer
        cmd->PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 0, 0, 1, &imageMemoryBarrier);

        Layout = TargetLayout;
    }

    void InsertImageMemoryBarrier(
        std::shared_ptr<CommandBuffer> cmd,
        VkAccessFlags                  srcAccessMask,
        VkAccessFlags                  dstAccessMask,
        VkImageLayout                  newImageLayout,
        VkPipelineStageFlags           srcStageMask,
        VkPipelineStageFlags           dstStageMask,
        VkImageSubresourceRange        subresourceRange)
    {

        VkImageMemoryBarrier imageMemoryBarrier = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask    = srcAccessMask,
            .dstAccessMask    = dstAccessMask,
            .oldLayout        = Layout,
            .newLayout        = newImageLayout,
            .image            = Handle,
            .subresourceRange = subresourceRange,
        };

        cmd->PipelineBarrier(srcStageMask, dstStageMask, 0, 0, 0, 0, 0, 1, &imageMemoryBarrier);
        Layout = newImageLayout;
    }

    void Upload(std::shared_ptr<VulkanAllocator> allocator, std::shared_ptr<CommandPool> Pool, u8* data, u64 sz)
    {
        assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        std::shared_ptr<VulkanBuffer> StagingBuffer = std::make_shared<VulkanBuffer>(allocator, sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);

        memcpy(StagingBuffer->Mapping, data, sz);

        Pool->Exec([&](std::shared_ptr<CommandBuffer> Cmd) {
            Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copy = {
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
                .imageExtent = {
                    .width  = Extent.width,
                    .height = Extent.height,
                    .depth  = 1,
                },
            };

            Cmd->CopyBufferToImage(StagingBuffer->Handle, Handle, Layout, 1, &copy);

            Transition(Cmd, FinalLayout);
        });
    }

    std::shared_ptr<VulkanBuffer> Download(std::shared_ptr<VulkanAllocator> allocator, std::shared_ptr<CommandPool> Pool)
    {

        std::shared_ptr<VulkanBuffer> StagingBuffer = std::make_shared<VulkanBuffer>(allocator, Allocation.Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);

        Pool->Exec([&](std::shared_ptr<CommandBuffer> Cmd) {
            Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            VkBufferImageCopy region = {
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
                .imageExtent = {Extent.width, Extent.height, 1},
            };
            Cmd->CopyImageToBuffer(Handle, Layout, StagingBuffer->Handle, 1, &region);

            Transition(Cmd, FinalLayout);
        });

        return StagingBuffer;
    }

    VulkanImage(std::shared_ptr<VulkanAllocator> allocator, ImageCreateInfo const& createInfo)
        : Vk(allocator->Vk),
          Extent(createInfo.Extent),
          FinalLayout(createInfo.FinalLayout),
          Layout(VK_IMAGE_LAYOUT_UNDEFINED),
          Format(createInfo.Format),
          Usage(createInfo.Usage),
          MipLevels(createInfo.MipLevels)
    {
        VkExternalMemoryImageCreateInfo resourceCreateInfo = {
            .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
        };

        VkImageCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            // .pNext       = &resourceCreateInfo,
            .imageType   = VK_IMAGE_TYPE_2D,
            .format      = Format,
            .extent      = {Extent.width, Extent.height, 1},
            .mipLevels   = MipLevels,
            .arrayLayers = 1,
            .samples     = VK_SAMPLE_COUNT_1_BIT,
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage       = Usage,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateImage(&info, 0, &Handle));

        Allocation = allocator->AllocateResourceMemory(Handle);

        VkImageViewCreateInfo viewInfo = {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = Handle,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = Format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = MipLevels,
                .layerCount = 1,
            },
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateImageView(&viewInfo, 0, &View));

        VkSamplerCreateInfo samplerInfo = {
            .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter        = VK_FILTER_LINEAR,
            .minFilter        = VK_FILTER_LINEAR,
            .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias       = 0.0f,
            .anisotropyEnable = 0,
            .maxAnisotropy    = 16,
            .compareOp        = VK_COMPARE_OP_NEVER,
            .maxLod           = (f32)MipLevels,
            .borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSampler(&samplerInfo, 0, &Sampler));
    }
};