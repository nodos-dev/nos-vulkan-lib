
#include <NativeAPIDirectx.h>

#include <Image.h>

#include <Device.h>

#include <Command.h>

#include <Buffer.h>

#undef CreateSemaphore

namespace mz::vk
{
Semaphore::Semaphore(Device* Vk, u64 pid, HANDLE ext) : 
  DeviceChild(Vk)
{
    VkExportSemaphoreWin32HandleInfoKHR handleInfo = {
        .sType    = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
        .dwAccess = GENERIC_ALL,
    };

    VkExportSemaphoreCreateInfo exportInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
        .pNext       = &handleInfo,
        .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
    };

    VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext         = &exportInfo,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeInfo,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSemaphore(&semaphoreCreateInfo, 0, &Handle));

    // Atil:
    // We want semaphores to be signaled when the image is ready to be used, which it is when it's first created.
    // Semaphores are created in the unsignaled state, so we need to signal them.
    // below is the current way of doing this
    if (!ext)
    {
        // u64 val = 0;
        // VkTimelineSemaphoreSubmitInfo timelineInfo = {
        //   .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        //   .signalSemaphoreValueCount = 1,
        //   .pSignalSemaphoreValues = &val,
        // };
        // VkSubmitInfo submitInfo = {
        //     .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        //     .pNext                = &timelineInfo,
        //     .signalSemaphoreCount = 1,
        //     .pSignalSemaphores    = &Handle,
        // };
        // MZ_VULKAN_ASSERT_SUCCESS(Vk->ImmCmdPool->Submit(1, &submitInfo, 0));
    }
    else
    {
        HANDLE sync = PlatformDupeHandle(pid, ext);

        VkImportSemaphoreWin32HandleInfoKHR importInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            .semaphore  = Handle,
            .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
            .handle     = sync,
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->ImportSemaphoreWin32HandleKHR(&importInfo));
    }

    VkSemaphoreGetWin32HandleInfoKHR getHandleInfo = {
        .sType      = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
        .semaphore  = Handle,
        .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->GetSemaphoreWin32HandleKHR(&getHandleInfo, &OSHandle));
    assert(OSHandle);

    DWORD flags;
    WIN32_ASSERT(GetHandleInformation(OSHandle, &flags));
}

Semaphore::operator VkSemaphore() const
{
    return Handle;
}

void Semaphore::Free()
{
    PlatformCloseHandle(OSHandle);
    Vk->DestroySemaphore(Handle, 0);
}

u64 Semaphore::GetValue() const
{
    u64 val;
    MZ_VULKAN_ASSERT_SUCCESS(Vk->GetSemaphoreCounterValue(Handle, &val));
    return val;
}

Semaphore::Semaphore(Device* Vk, const MemoryExportInfo* Imported)
    : Semaphore(Vk, Imported ? Imported->PID : 0, Imported ? Imported->Sync : 0)
{
}

Image::~Image()
{
    Allocation.Free();
    Sema.Free();
    Vk->DestroyImageView(View, 0);
    Vk->DestroySampler(Sampler, 0);
    Vk->DestroyImage(Handle, 0);
};

Image::Image(Allocator* Allocator, ImageCreateInfo const& createInfo)
    : DeviceChild(Allocator->Vk),
      Extent(createInfo.Extent),
      Format(createInfo.Format),
      Usage(createInfo.Usage),
      Sema(Vk, createInfo.Imported),
      State{
          .StageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .AccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
          .Layout     = VK_IMAGE_LAYOUT_UNDEFINED,
      }
{
    if (createInfo.Imported)
    {
        State.Layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    }

    assert(IsImportable(Vk->PhysicalDevice, Format, Usage, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT));

    VkExternalMemoryImageCreateInfo resourceCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
    };

    VkImageCreateInfo info = {
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = &resourceCreateInfo,
        .flags                 = VK_IMAGE_CREATE_ALIAS_BIT,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = Format,
        .extent                = {Extent.width, Extent.height, 1},
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = Usage,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateImage(&info, 0, &Handle));

    Allocation = Allocator->AllocateImageMemory(Handle, Extent, Format, createInfo.Imported);
    Allocation.BindResource(Handle);

    VkImageViewUsageCreateInfo usageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .usage = this->Usage,
    };

    VkImageViewCreateInfo viewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext      = &usageInfo,
        .image      = Handle,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = Format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
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

void Image::Transition(
    rc<CommandBuffer> Cmd,
    ImageState Dst)
{
    // Dst.AccessMask = 0;
    // Dst.StageMask  = 0;
    ImageLayoutTransition(this->Handle, Cmd, this->State, Dst);
    Cmd->AddDependency(shared_from_this());
    State = Dst;
}

void Image::Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    assert(Src->Usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    Transition(Cmd, ImageState{
                        .StageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .Layout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    });

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

    Cmd->CopyBufferToImage(Src->Handle, Handle, State.Layout, 1, &region);

    // make sure the buffer is alive until after the command buffer has finished
    Cmd->AddDependency(Src);
}

rc<Image> Image::Copy(rc<CommandBuffer> Cmd, rc<Allocator> Allocator)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    if (0 == Allocator)
    {
        Allocator = Vk->ImmAllocator;
    }

    rc<Image> Img = Image::New(Allocator.get(), ImageCreateInfo{
                                                    .Extent = Extent,
                                                    .Format = Format,
                                                    .Usage  = Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                });

    Img->Transition(Cmd, ImageState{
                             .StageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
                             .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                             .Layout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         });
    this->Transition(Cmd, ImageState{
                              .StageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
                              .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                              .Layout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          });

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

    return Img;
}

rc<Buffer> Image::Download(rc<CommandBuffer> Cmd, rc<Allocator> Allocator)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    if (0 == Allocator)
    {
        Allocator = Vk->ImmAllocator;
    }

    rc<Buffer> StagingBuffer = Buffer::New(Allocator.get(), Allocation.LocalSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, Buffer::Heap::CPU);

    Transition(Cmd, ImageState{
                        .StageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                        .Layout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    });

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

    Cmd->CopyImageToBuffer(Handle, State.Layout, StagingBuffer->Handle, 1, &region);

    return StagingBuffer;
}

void Image::BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src)
{
    Image* Dst = this;

    VkImageBlit blit = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .srcOffsets     = {{}, {(i32)Src->Extent.width, (i32)Src->Extent.height, 1}},
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .dstOffsets = {{}, {(i32)Dst->Extent.width, (i32)Dst->Extent.height, 1}},
    };

    Src->Transition(Cmd, ImageState{
                             .StageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
                             .AccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                             .Layout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         });

    Dst->Transition(Cmd, ImageState{
                             .StageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
                             .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                             .Layout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         });

    Cmd->BlitImage(Src->Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Dst->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    Cmd->AddDependency(Src, shared_from_this());
}

MemoryExportInfo Image::GetExportInfo() const
{
    return MemoryExportInfo{
        .PID    = PlatformGetCurrentProcessId(),
        .Memory = Allocation.GetOSHandle(),
        .Sync   = Sema.OSHandle,
        .Offset = Allocation.GlobalOffset(),
    };
}

Image::Image(Device* Vk, ImageCreateInfo const& createInfo)
    : Image(Vk->ImmAllocator.get(), createInfo)
{
}

DescriptorResourceInfo Image::GetDescriptorInfo() const
{
    return DescriptorResourceInfo{
        .Image = {
            .sampler     = Sampler,
            .imageView   = View,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }};
}

} // namespace mz::vk