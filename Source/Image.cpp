#include "vulkan/vulkan_core.h"
#include <Image.h>
#include <Device.h>
#include <Command.h>
#include <Buffer.h>
#include <unordered_map>

namespace mz::vk
{

Sampler::Sampler(Device* Vk, VkSamplerYcbcrConversion SamplerYcbcrConversion)
{
    VkSamplerYcbcrConversionInfo ycbcrInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
        .conversion = SamplerYcbcrConversion,
    };
    
    VkSamplerCreateInfo samplerInfo = {
        .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext            = SamplerYcbcrConversion ? &ycbcrInfo : 0,
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

    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSampler(&samplerInfo, 0, &Handle));
}

void Sampler::Free(Device* Vk)
{
    if(Handle)
    {
        Vk->DestroySampler(Handle, 0);
    }
}

Image::~Image()
{
    Allocation.Free();
    Sampler.Free(Vk);
    Vk->DestroyImageView(View, 0);
    Vk->DestroyImage(Handle, 0);
};

Image::Image(Allocator* Allocator, ImageCreateInfo const& createInfo)
    : DeviceChild(Allocator->Vk),
      Extent(createInfo.Extent),
      Format(createInfo.Format),
      Usage(createInfo.Usage),
      SamplerYcbcrConversion(0),
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
    
    static std::map<VkFormat, VkSamplerYcbcrConversion>  ycbr;

    switch(Format)
    {
        case VK_FORMAT_G8B8G8R8_422_UNORM:
        case VK_FORMAT_B8G8R8G8_422_UNORM:
        case VK_FORMAT_R10X6_UNORM_PACK16:
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        case VK_FORMAT_R12X4_UNORM_PACK16:
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        case VK_FORMAT_G16B16G16R16_422_UNORM:
        case VK_FORMAT_B16G16R16G16_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
            if(ycbr[Format])
            {
                SamplerYcbcrConversion= ycbr[Format];   
            }
            else 
            {
                MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateSamplerYcbcrConversion(&ycbcrCreateInfo, 0, &SamplerYcbcrConversion));
                ycbr[Format] = SamplerYcbcrConversion;
            }
        default:
            break;
    }

    VkSamplerYcbcrConversionInfo ycbcrInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
        .conversion = SamplerYcbcrConversion,
    };

    VkImageViewUsageCreateInfo usageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .pNext = SamplerYcbcrConversion ? &ycbcrInfo : 0,
        .usage = this->Usage,
    };


    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateImage(&info, 0, &Handle));
    //if(SamplerYcbcrConversion) Allocation = Allocator->AllocateResourceMemory(Handle, true, createInfo.Imported); else 
        Allocation = Allocator->AllocateImageMemory(Handle, Extent, Format, createInfo.Imported);
    Allocation.BindResource(Handle);
    
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

    Sampler = vk::Sampler(Vk, SamplerYcbcrConversion);

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
            .width  = Extent.width,
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
    VkImageBlit2 region = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
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

void Image::CopyFrom(rc<CommandBuffer> Cmd, rc<Image> Src)
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
        .extent = {Extent.width, Extent.height, 1},
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
        .Type   = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
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