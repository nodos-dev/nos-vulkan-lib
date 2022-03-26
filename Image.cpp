
#include <NativeAPID3D12.h>

#include <Image.h>

#undef CreateSemaphore

namespace mz::vk
{

Image::Image(Allocator* Allocator, ImageCreateInfo const& createInfo)
    : Vk(Allocator->GetDevice()),
      Extent(createInfo.Extent),
      Layout(VK_IMAGE_LAYOUT_UNDEFINED),
      Format(createInfo.Format),
      Usage(createInfo.Usage),
      Sync(0),
      AccessMask(0)
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

    if (createInfo.Exported)
    {
        VkImportSemaphoreWin32HandleInfoKHR importInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            .semaphore  = Sema,
            .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            .handle     = createInfo.Exported->sync,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->ImportSemaphoreWin32HandleKHR(&importInfo));

        AccessMask = createInfo.Exported->accessMask;
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

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateImage(&info, 0, &Handle));

    Allocation = Allocator->AllocateResourceMemory(Handle, false, createInfo.Exported);

    Allocation.BindResource(Handle);

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

} // namespace mz::vk

void Image::Transition(VkImageLayout TargetLayout, VkAccessFlags TargetAccessMask)
{
    auto Cmd = Vk->ImmCmdPool->BeginCmd();

    ImageLayoutTransition(Handle, Cmd, Layout, TargetLayout, AccessMask, TargetAccessMask);

    Cmd->Submit(shared_from_this(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    Layout     = TargetLayout;
    AccessMask = TargetAccessMask;
}

void Image::Transition(
    std::shared_ptr<CommandBuffer> Cmd,
    VkImageLayout TargetLayout,
    VkAccessFlags TargetAccessMask)
{

    ImageLayoutTransition(Handle, Cmd, Layout, TargetLayout, AccessMask, TargetAccessMask);

    Layout     = TargetLayout;
    AccessMask = TargetAccessMask;
}

void Image::Upload(u8* data, Allocator* Allocator, CommandPool* Pool)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    if (0 == Allocator)
    {
        Allocator = Vk->ImmAllocator.get();
    }

    u64 Size                              = Extent.width * Extent.height * 4;
    std::shared_ptr<Buffer> StagingBuffer = Buffer::New(Allocator, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, Buffer::Heap::CPU);
    memcpy(StagingBuffer->Map(), data, Size);
    StagingBuffer->Flush();
    Upload(StagingBuffer, Pool);

} // namespace mz::vk

void Image::Upload(std::shared_ptr<Buffer> StagingBuffer, CommandPool* Pool)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    if (0 == Pool)
    {
        Pool = Vk->ImmCmdPool.get();
    }

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

    Cmd->Submit(shared_from_this(), VK_PIPELINE_STAGE_TRANSFER_BIT);

    Cmd->Wait();
}

std::shared_ptr<Image> Image::Copy(Allocator* Allocator, CommandPool* Pool)
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

    std::shared_ptr<Image> Img = Image::New(
        Allocator, ImageCreateInfo{
                       .Extent = Extent,
                       .Format = Format,
                       .Usage  = Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                   });

    std::shared_ptr<CommandBuffer> Cmd = Pool->BeginCmd();

    {
        Img->Transition(Cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
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

        Cmd->CopyImage(Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Img->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    std::shared_ptr<Image> Res[2] = {Img, shared_from_this()};
    Cmd->Submit(Res, VK_PIPELINE_STAGE_TRANSFER_BIT);
    Cmd->Wait();

    return Img;
}

std::shared_ptr<Buffer> Image::Download(Allocator* Allocator, CommandPool* Pool)
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

    std::shared_ptr<Buffer> StagingBuffer = Buffer::New(Allocator, Allocation.Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, Buffer::Heap::CPU);

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

    Cmd->Submit(shared_from_this(), VK_PIPELINE_STAGE_TRANSFER_BIT);
    Cmd->Wait();

    return StagingBuffer;
}

ImageExportInfo Image::GetExportInfo() const
{
    return ImageExportInfo{
        .memory     = Allocation.GetOSHandle(),
        .sync       = Sync,
        .offset     = Allocation.Offset + Allocation.Block->Offset,
        .size       = Allocation.Block->Size,
        .accessMask = AccessMask,
    };
}

DescriptorResourceInfo Image::GetDescriptorInfo() const
{
    return DescriptorResourceInfo{
        .image = {
            .sampler     = Sampler,
            .imageView   = View,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }};
}

Image::Image(Device* Vk, ImageCreateInfo const& createInfo)
    : Image(Vk->ImmAllocator.get(), createInfo)
{
}

Image::~Image()
{
    if (!Allocation.IsImported())
    {
        assert(PlatformClosehandle(Sync));
    }
    Vk->DestroySemaphore(Sema, 0);
    Vk->DestroyImage(Handle, 0);
    Vk->DestroyImageView(View, 0);
    Vk->DestroySampler(Sampler, 0);
    Allocation.Free();
};

} // namespace mz::vk