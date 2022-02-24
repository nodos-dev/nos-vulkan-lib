
#include "Image.h"
#include "InfoStructs.h"

#include "NativeAPID3D12.h"
#include "vulkan/vulkan_core.h"

namespace mz
{
static bool IsImportable(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage)
{
    VkPhysicalDeviceExternalImageFormatInfo externalimageFormatInfo = {
        .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        .sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
        .pNext  = &externalimageFormatInfo,
        .format = Format,
        .type   = VK_IMAGE_TYPE_2D,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage  = Usage,
        .flags  = VK_IMAGE_CREATE_ALIAS_BIT,
    };

    VkExternalImageFormatProperties extProps = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
    };

    VkImageFormatProperties2 props = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        .pNext = &extProps,
    };

    MZ_VULKAN_ASSERT_SUCCESS(vkGetPhysicalDeviceImageFormatProperties2(PhysicalDevice, &imageFormatInfo, &props));

    assert(!(extProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT));

    return extProps.externalMemoryProperties.externalMemoryFeatures & (VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT | VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT);
}

VulkanImage::VulkanImage(VulkanDevice* Vk, ImageCreateInfo const& createInfo)
    : VulkanImage(Vk->ImmAllocator.get(), createInfo)
{
}

VulkanImage::VulkanImage(VulkanAllocator* Allocator, ImageCreateInfo const& createInfo)
    : Vk(Allocator->GetDevice()),
      Extent(createInfo.Extent),
      Layout(VK_IMAGE_LAYOUT_UNDEFINED),
      Format(createInfo.Format),
      Usage(createInfo.Usage),
      Sync(createInfo.Ext.sync),
      AccessMask(createInfo.Ext.accessMask)
{

    Vk->DeviceWaitIdle();

    assert(IsImportable(Vk->PhysicalDevice, Format, Usage));

    VkExportSemaphoreWin32HandleInfoKHR handleInfo = {
        .sType    = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
        .dwAccess = GENERIC_ALL,
    };

    VkExportSemaphoreCreateInfo exportInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .pNext       = &handleInfo,
        .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext         = &exportInfo,
        .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSemaphore(&semaphoreCreateInfo, 0, &Sema));

    if (createInfo.Ext.sync)
    {
        VkImportSemaphoreWin32HandleInfoKHR importInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            .semaphore  = Sema,
            .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            .handle     = createInfo.Ext.sync,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->ImportSemaphoreWin32HandleKHR(&importInfo));
    }

    VkSemaphoreGetWin32HandleInfoKHR getHandleInfo = {
        .sType      = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore  = Sema,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->GetSemaphoreWin32HandleKHR(&getHandleInfo, &Sync));
    assert(Sync);

    DWORD flags;
    WIN32_ASSERT(GetHandleInformation(Sync, &flags));

    VkExternalMemoryImageCreateInfo resourceCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };

    VkImageCreateInfo info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext         = &resourceCreateInfo,
        .flags         = VK_IMAGE_CREATE_ALIAS_BIT,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = Format,
        .extent        = {Extent.width, Extent.height, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = Usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (createInfo.Ext.memory)
    {
        info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        Layout             = VK_IMAGE_LAYOUT_PREINITIALIZED;
    }

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateImage(&info, 0, &Handle));

    Allocation = Allocator->AllocateResourceMemory(Handle, false, createInfo.Ext);

    MZ_VULKAN_ASSERT_SUCCESS(Vk->BindImageMemory(Handle, Allocation.Block->Memory, Allocation.Offset + createInfo.Ext.offset));

    VkImageViewCreateInfo viewInfo = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = Handle,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = Format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
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
        .maxLod           = 1.f,
        .borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSampler(&samplerInfo, 0, &Sampler));

} // namespace mz

VulkanImage::~VulkanImage()
{
    if (!Allocation.IsImported())
    {
        assert(SUCCEEDED(CloseHandle(Sync)));
    }
    Vk->DestroyImage(Handle, 0);
    Vk->DestroyImageView(View, 0);
    Vk->DestroySampler(Sampler, 0);
    Vk->DestroySemaphore(Sema, 0);
    Allocation.Free();
}

void ImageLayoutTransition(VkImage                        Image,
                           std::shared_ptr<CommandBuffer> Cmd,
                           VkImageLayout                  CurrentLayout,
                           VkImageLayout                  TargetLayout,
                           VkAccessFlags                  srcAccessMask,
                           VkAccessFlags                  dstAccessMask)
{
    // Create an image barrier object
    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = srcAccessMask,
        .dstAccessMask       = dstAccessMask,
        .oldLayout           = CurrentLayout,
        .newLayout           = TargetLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = Image,
        .subresourceRange    = {
               .aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT,
               .baseMipLevel = 0,
               .levelCount   = 1,
               .layerCount   = 1,
        },
    };

    {

        // // https://github.com/SaschaWillems/Vulkan/blob/821a0659a76131662b1fc4a77c5a1ee6a9a330d8/base/VulkanTools.cpp#L142

        // // Source layouts (old)
        // // Source access mask controls actions that have to be finished on the old layout
        // // before it will be transitioned to the new layout
        // switch (CurrentLayout)
        // {
        // case VK_IMAGE_LAYOUT_UNDEFINED:
        //     // Image layout is undefined (or does not matter)
        //     // Only valid as initial layout
        //     // No flags required, listed only for completeness
        //     imageMemoryBarrier.srcAccessMask = 0;
        //     break;

        // case VK_IMAGE_LAYOUT_PREINITIALIZED:
        //     // Image is preinitialized
        //     // Only valid as initial layout for linear images, preserves memory contents
        //     // Make sure host writes have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        //     // Image is a color attachment
        //     // Make sure any writes to the color buffer have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        //     // Image is a depth/stencil attachment
        //     // Make sure any writes to the depth/stencil buffer have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        //     // Image is a transfer source
        //     // Make sure any reads from the image have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        //     // Image is a transfer destination
        //     // Make sure any writes to the image have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        //     // Image is read by a shader
        //     // Make sure any shader reads from the image have been finished
        //     imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        //     break;
        // default:
        //     // Other source layouts aren't handled (yet)
        //     break;
        // }

        // // Target layouts (new)
        // // Destination access mask controls the dependency for the new image layout
        // switch (TargetLayout)
        // {
        // case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        //     // Image will be used as a transfer destination
        //     // Make sure any writes to the image have been finished
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        //     // Image will be used as a transfer source
        //     // Make sure any reads from the image have been finished
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        //     // Image will be used as a color attachment
        //     // Make sure any writes to the color buffer have been finished
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        //     // Image layout will be used as a depth/stencil attachment
        //     // Make sure any writes to depth/stencil buffer have been finished
        //     imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        //     break;

        // case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        //     // Image will be read in a shader (sampler, input attachment)
        //     // Make sure any writes to the image have been finished
        //     if (imageMemoryBarrier.srcAccessMask == 0)
        //     {
        //         imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        //     }
        //     imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        //     break;
        // default:
        //     // Other source layouts aren't handled (yet)
        //     break;
        // }
    }

    // Put barrier inside setup command buffer
    Cmd->PipelineBarrier(
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0,
        0,
        0,
        0,
        1,
        &imageMemoryBarrier);
}

void VulkanImage::Transition(VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask)
{
    auto Cmd = Vk->ImmCmdPool->BeginCmd();

    ImageLayoutTransition(Handle, Cmd, Layout, TargetLayout, AccessMask, TargetAccessMask);

    Cmd->Submit(this, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    Layout     = TargetLayout;
    AccessMask = TargetAccessMask;
}

void VulkanImage::Transition(
    std::shared_ptr<CommandBuffer> Cmd,
    VkImageLayout                  TargetLayout,
    VkAccessFlags                  TargetAccessMask)
{

    ImageLayoutTransition(Handle, Cmd, Layout, TargetLayout, AccessMask, TargetAccessMask);

    Layout     = TargetLayout;
    AccessMask = TargetAccessMask;
}

void VulkanImage::Upload(u8* data, VulkanAllocator* Allocator, CommandPool* Pool)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    if (0 == Allocator)
    {
        Allocator = Vk->ImmAllocator.get();
    }

    if (0 == Pool)
    {
        Pool = Vk->ImmCmdPool.get();
    }

    u64 Size = Extent.width * Extent.height * 4;

    std::shared_ptr<VulkanBuffer> StagingBuffer = MakeShared<VulkanBuffer>(Allocator, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    memcpy(StagingBuffer->Map(), data, Size);

    StagingBuffer->Flush();

    std::shared_ptr<CommandBuffer> Cmd = Pool->BeginCmd();

    {
        Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkBufferImageCopy region = {
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

        Cmd->CopyBufferToImage(StagingBuffer->Handle, Handle, Layout, 1, &region);
    }

    Cmd->Submit(this, VK_PIPELINE_STAGE_TRANSFER_BIT);
    Cmd->Wait();

} // namespace mz

std::shared_ptr<VulkanImage> VulkanImage::Copy(VulkanAllocator* Allocator, CommandPool* Pool)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    if (0 == Allocator)
    {
        Allocator = Vk->ImmAllocator.get();
    }

    if (0 == Pool)
    {
        Pool = Vk->ImmCmdPool.get();
    }

    std::shared_ptr<VulkanImage> Image = MakeShared<VulkanImage>(
        Allocator, ImageCreateInfo{
                       .Extent = Extent,
                       .Format = Format,
                       .Usage  = Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                   });

    std::shared_ptr<CommandBuffer> Cmd = Pool->BeginCmd();

    {
        Image->Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
        Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);

        VkImageCopy region = {
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .extent = {Extent.width, Extent.height, 1},
        };

        Cmd->CopyImage(Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Image->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    Cmd->Submit({Image.get(), this}, VK_PIPELINE_STAGE_TRANSFER_BIT);
    Cmd->Wait();

    return Image;
}

std::shared_ptr<VulkanBuffer> VulkanImage::Download(VulkanAllocator* Allocator, CommandPool* Pool)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    if (0 == Allocator)
    {
        Allocator = Vk->ImmAllocator.get();
    }

    if (0 == Pool)
    {
        Pool = Vk->ImmCmdPool.get();
    }

    std::shared_ptr<VulkanBuffer> StagingBuffer = MakeShared<VulkanBuffer>(Allocator, Allocation.Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    std::shared_ptr<CommandBuffer> Cmd = Pool->BeginCmd();

    {
        Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);

        VkBufferImageCopy region = {
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

        Cmd->CopyImageToBuffer(Handle, Layout, StagingBuffer->Handle, 1, &region);
    }

    Cmd->Submit(this, VK_PIPELINE_STAGE_TRANSFER_BIT);
    Cmd->Wait();

    return StagingBuffer;
}
} // namespace mz