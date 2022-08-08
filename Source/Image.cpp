#include "vulkan/vulkan_core.h"
#include <Image.h>
#include <Device.h>
#include <Command.h>
#include <Buffer.h>
#include <unordered_map>

namespace mz::vk
{

Sampler::Sampler(Device* Vk, VkFormat Format) : SamplerYcbcrConversion(0)
{
    VkSamplerYcbcrConversionCreateInfo ycbcrCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
        .pNext = 0,
        .format = Format,
        .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
        .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_ONE,
        },
        .xChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN,
        .yChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN,
        .chromaFilter = VK_FILTER_NEAREST,
        .forceExplicitReconstruction = VK_FALSE,
    };
    VkSamplerCreateInfo samplerInfo = {
            .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter        = VK_FILTER_NEAREST,
            .minFilter        = VK_FILTER_NEAREST,
            .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias       = 0.0f,
            .anisotropyEnable = 0,
            .maxAnisotropy    = 16,
            .compareOp        = VK_COMPARE_OP_NEVER,
            .maxLod           = 1.f,
            .borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        };

    static std::mutex Mutex;
    static std::map<VkFormat, std::pair<VkSamplerYcbcrConversion, VkSampler>>  ycbr;
    std::unique_lock lock(Mutex);
    //if (IsYCbCr(Format))
    //{
    //    if (ycbr.contains(Format))
    //    {
    //        auto& [cvt, sampler] = ycbr[Format];
    //        SamplerYcbcrConversion = cvt;
    //        Handle = sampler;
    //    }
    //    else
    //    {
    //        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSamplerYcbcrConversion(&ycbcrCreateInfo, 0, &SamplerYcbcrConversion));
    //        VkSamplerYcbcrConversionInfo ycbcrInfo = {
    //            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
    //            .conversion = SamplerYcbcrConversion,
    //        };
    //        samplerInfo.pNext = &ycbcrInfo;
    //        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSampler(&samplerInfo, 0, &Handle));
    //        ycbr[Format] = { SamplerYcbcrConversion, Handle };
    //    }
    //    return;
    //}

    static VkSampler sampler = 0;

    if(!sampler)
    {

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSampler(&samplerInfo, 0, &sampler));
    }

    Handle = sampler;
}

Image::~Image()
{
    Views.clear();
    Allocation.Free();
    Vk->DestroyImage(Handle, 0);
};

ImageView::~ImageView()
{
    //Src->Views.erase(Hash()); //QUESTION: Is it necessary?
    Vk->DestroyImageView(Handle, 0);
}

ImageView::ImageView(Device* Vk, struct Image* Src, VkFormat Format, VkImageUsageFlags Usage) :
    DeviceChild(Vk), Src(Src), Format(Format ? Format : Src->GetFormat()), Usage(Usage ? Usage : Src->Usage), Sampler(Src->GetDevice(), Src->GetFormat())
{
    VkSamplerYcbcrConversionInfo ycbcrInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
        .conversion = Sampler.SamplerYcbcrConversion,
    };

    VkImageViewUsageCreateInfo usageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .pNext = Sampler.SamplerYcbcrConversion ? &ycbcrInfo : 0,
        .usage = this->Usage,
    };

    VkImageViewCreateInfo viewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext      = &usageInfo,
        .image      = Src->Handle,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = IsYCbCr(this->Format) ? VK_FORMAT_R8G8B8A8_UNORM : this->Format,
        .components = {},
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    MZ_VULKAN_ASSERT_SUCCESS(Src->GetDevice()->CreateImageView(&viewInfo, 0, &Handle));
}

Image::Image(Allocator* Allocator, ImageCreateInfo const& createInfo)
    : DeviceChild(Allocator->Vk),
      Extent(createInfo.Extent),
      Format(createInfo.Format),
      Usage(createInfo.Usage),
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

    // assert(IsImportable(Vk->PhysicalDevice, Format, Usage, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT));

    VkExternalMemoryImageCreateInfo resourceCreateInfo = {
        .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = (VkFlags)createInfo.Type,
    };

    VkImageCreateInfo info = {
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
         .pNext                 = &resourceCreateInfo,
        .flags                 = createInfo.Flags,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = GetEffectiveFormat(),
        .extent                = {GetEffectiveExtent().width, Extent.height, 1},
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = createInfo.Tiling, //IsYCbCr(Format) ? VK_IMAGE_TILING_LINEAR : createInfo.Tiling,
        .usage                 = Usage,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateImage(&info, 0, &Handle));
    //if(SamplerYcbcrConversion) Allocation = Allocator->AllocateResourceMemory(Handle, true, createInfo.Imported); else 
    Allocation = Allocator->AllocateImageMemory(Handle, createInfo);
    Allocation.BindResource(Handle);
} // namespace mz::vk

void Image::Transition(
    rc<CommandBuffer> Cmd,
    ImageState Dst)
{

    // Dst.AccessMask = 0;
    // Dst.StageMask  = 0;
    if (Vk->FallbackOptions.mzSync2Fallback)
    {
        ImageLayoutTransition(this->Handle, Cmd, this->State, Dst);
    }
    else 
    {
        ImageLayoutTransition2(this->Handle, Cmd, this->State, Dst);
    }
    Cmd->AddDependency(shared_from_this());
    State = Dst;
}

void Image::Upload(rc<CommandBuffer> Cmd, rc<Buffer> Src, u32 bufferRowLength, u32 bufferImageHeight)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    assert(Src->Usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    // make sure the buffer is alive until after the command buffer has finished
    Cmd->AddDependency(Src);
    
    Transition(Cmd, ImageState{
                        .StageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
                        .AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .Layout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    });

    VkBufferImageCopy region = {
        .bufferRowLength = bufferRowLength,
        .bufferImageHeight = bufferImageHeight,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .imageExtent = {
            .width  = GetEffectiveExtent().width,
            .height = Extent.height,
            .depth  = 1,
        },
    };

    Cmd->CopyBufferToImage(Src->Handle, Handle, State.Layout, 1, &region);

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
        .extent = {GetEffectiveExtent().width, Extent.height, 1},
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

    rc<Buffer> StagingBuffer = Buffer::New(Allocator.get(), BufferCreateInfo { 
        .Size = (u32)Allocation.LocalSize(), 
        .Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
    });
    
    Download(Cmd, StagingBuffer);
    return StagingBuffer;
}

void Image::Download(rc<CommandBuffer> Cmd, rc<Buffer> Buffer)
{
    // assert(Buffer->Allocation.LocalSize() >= Allocation.LocalSize());
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
            .width  = GetEffectiveExtent().width,
            .height = Extent.height,
            .depth  = 1,
        },
    };

    Cmd->CopyImageToBuffer(Handle, State.Layout, Buffer->Handle, 1, &region);
    Cmd->AddDependency(shared_from_this(), Buffer);
}

void Image::BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src)
{
    Image* Dst = this;

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

    if (Vk->FallbackOptions.mzCopy2Fallback)
    {
        VkImageBlit region = {
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .srcOffsets = {{}, {(i32)Src->Extent.width / (IsYCbCr(Src->Format) + 1), (i32)Src->Extent.height, 1}},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .dstOffsets = {{}, {(i32)Dst->Extent.width / (IsYCbCr(Dst->Format) + 1), (i32)Dst->Extent.height, 1}},
        };
        Cmd->BlitImage(Src->Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Dst->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1 , &region, VK_FILTER_NEAREST);
    }
    else
    {
        VkImageBlit2 region = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .srcOffsets = {{}, {(i32)Src->Extent.width / (IsYCbCr(Src->Format) + 1), (i32)Src->Extent.height, 1}},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .dstOffsets = {{}, {(i32)Dst->Extent.width / (IsYCbCr(Dst->Format) + 1), (i32)Dst->Extent.height, 1}},
        };

        VkBlitImageInfo2 blitInfo = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = Src->Handle,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = Dst->Handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &region,
            .filter = VK_FILTER_NEAREST,
        };

        Cmd->BlitImage2(&blitInfo);
    }
}

void Image::CopyFrom(rc<CommandBuffer> Cmd, rc<Image> Src)
{
    Image* Dst = this;
    assert(
        (Dst->Extent.width == Src->Extent.width && Dst->Extent.height == Src->Extent.height) ||
        Dst->Allocation.LocalSize() >= Src->Allocation.LocalSize()
    );

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

    VkImageCopy region = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .extent = {GetEffectiveExtent().width, Extent.height, 1},
    };

    Cmd->CopyImage(Src->Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Dst->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void Image::ResolveFrom(rc<CommandBuffer> Cmd, rc<Image> Src)
{
    Image* Dst = this;
    
    assert(Dst->Extent.width == Src->Extent.width && Dst->Extent.height == Src->Extent.height);

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

    VkImageResolve2 region = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2_KHR,
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .extent = {GetEffectiveExtent().width, Extent.height, 1},
    };

    VkResolveImageInfo2 resolveInfo = {
        .sType = VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2,
        .srcImage = Src->Handle,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .dstImage = Dst->Handle,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &region,
    };

    Cmd->ResolveImage2(&resolveInfo);
}


MemoryExportInfo Image::GetExportInfo() const
{
    return MemoryExportInfo{
        .PID    = PlatformGetCurrentProcessId(),
        .Memory = Allocation.GetOSHandle(),
        .Type   = Allocation.GetType(),
        .Offset = Allocation.GlobalOffset(),
    };
}

Image::Image(Device* Vk, ImageCreateInfo const& createInfo)
    : Image(Vk->ImmAllocator.get(), createInfo)
{
}

DescriptorResourceInfo ImageView::GetDescriptorInfo() const
{
    return DescriptorResourceInfo{
        .Image = {
            .sampler     = Sampler,
            .imageView   = Handle,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }};
}

} // namespace mz::vk