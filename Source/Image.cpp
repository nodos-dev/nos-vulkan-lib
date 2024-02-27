// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "vulkan/vulkan_core.h"
#include <nosVulkan/Image.h>
#include <nosVulkan/Device.h>
#include <nosVulkan/Command.h>
#include <nosVulkan/Buffer.h>

namespace nos::vk
{

Image::~Image()
{
    Views.clear();
    if (Allocation.Imported)
        Vk->DestroyImage(Handle, 0);
    else if (Allocation.Handle)
        vmaDestroyImage(Vk->Allocator, Handle, Allocation.Handle);
};

ImageView::~ImageView()
{
    Vk->DestroyImageView(Handle, 0);
}

ImageView::ImageView(struct Image* Src, VkFormat Format, VkImageUsageFlags Usage) :
    DeviceChild(Src->GetDevice()), Src(Src), Format(Format ? Format : Src->GetFormat()), Usage(Usage ? Usage : Src->Usage)
{ 
    VkSamplerYcbcrConversionInfo ycbcrInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
        // .conversion = Sampler.SamplerYcbcrConversion,
    };

    VkImageViewUsageCreateInfo usageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        // .pNext = Sampler.SamplerYcbcrConversion ? &ycbcrInfo : 0,
        .usage = this->Usage,
    };

    VkImageViewCreateInfo viewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext      = &usageInfo,
        .image      = Src->Handle,
        .viewType   = VkImageViewType(Src->GetImageType()),
        .format     = IsYCbCr(this->Format) ? VK_FORMAT_R8G8B8A8_UNORM : this->Format,
        .components = {},
        .subresourceRange = {
            .aspectMask = Src->GetAspect(),
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    NOSVK_ASSERT(Src->GetDevice()->CreateImageView(&viewInfo, 0, &Handle));
}

Image::Image(Device* Vk, ImageCreateInfo const& createInfo, VkResult* re)
    : ResourceBase(Vk),
      Extent(createInfo.Extent),
      Format(createInfo.Format),
      Usage(createInfo.Usage),
      State{
          .StageMask  = VK_PIPELINE_STAGE_NONE,
          .AccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
          .Layout     = VK_IMAGE_LAYOUT_UNDEFINED,
      }
{
	if (createInfo.Imported)
		State.Layout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	// assert(IsImportable(Vk->PhysicalDevice, Format, Usage, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT));

	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(Vk->PhysicalDevice, GetEffectiveFormat(), &props);

	auto Ft = props.optimalTilingFeatures;
	bool Opt = true;
	VkImageTiling tiling = createInfo.Tiling;

	if (tiling == VK_IMAGE_TILING_OPTIMAL)
	{
		if (((Usage & VK_IMAGE_USAGE_SAMPLED_BIT) && !(Ft & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT)) ||
			((Usage & VK_IMAGE_USAGE_SAMPLED_BIT) && !(Ft & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT)) ||
			((Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) && !(Ft & VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT)) ||
			((Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) && !(Ft & VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT)) ||
			((Usage & VK_IMAGE_USAGE_SAMPLED_BIT) && !(Ft & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT)) ||
			((Usage & VK_IMAGE_USAGE_STORAGE_BIT) && !(Ft & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT)) ||
			((Usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) && !(Ft & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT)) ||
			((Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
			 !(Ft & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT)))
		{
			tiling = VK_IMAGE_TILING_LINEAR;
		}
	}

	VkExternalMemoryImageCreateInfo resourceCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		.handleTypes = createInfo.ExternalMemoryHandleType,
	};

	VkImageCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = &resourceCreateInfo,
		.flags = createInfo.Flags,
		.imageType = GetImageType(),
		.format = GetEffectiveFormat(),
		.extent = {GetEffectiveExtent().width, Extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = createInfo.Samples,
		.tiling = tiling,
		.usage = Usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkResult result;
	if (auto* imported = createInfo.Imported)
	{
        result = Vk->CreateImage(&info, 0, &Handle);
        if (NOS_VULKAN_SUCCEEDED(result))
            result = Allocation.Import(Vk, Handle, *imported, memProps);
	}
	else // Exported
	{
		VmaAllocationCreateInfo allocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO, .requiredFlags = memProps};
		result = vmaCreateImage(Vk->Allocator, &info, &allocationCreateInfo, &Handle, &Allocation.Handle, &Allocation.Info);
    }
    
	if (NOS_VULKAN_SUCCEEDED(result))
	{
		VkMemoryRequirements memReq = {};
        Vk->GetImageMemoryRequirements(Handle, &memReq);
		assert(memReq.size == Allocation.GetSize());
	}

	if (NOS_VULKAN_SUCCEEDED(result))
        result = Allocation.SetExternalMemoryHandleType(Vk, createInfo.ExternalMemoryHandleType);

	if (re)
		*re = result;
}

void Image::Transition(
    rc<CommandBuffer> Cmd,
    ImageState Dst)
{
    // Dst.AccessMask = 0;
    // Dst.StageMask  = 0;
    if (!Vk->Features.synchronization2)
    {
        ImageLayoutTransition(Handle, Cmd, State, Dst, GetAspect());
    }
    else 
    {
        ImageLayoutTransition2(Handle, Cmd, State, Dst, GetAspect());
    }
    State = Dst;
    Cmd->AddDependency(shared_from_this());
}

void Image::Clear(rc<CommandBuffer> Cmd, VkClearColorValue value)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    Transition(Cmd, ImageState{
                        .AccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                        .Layout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    });
    VkImageSubresourceRange range = {
        .aspectMask = GetAspect(),
        .levelCount = 1,
        .layerCount = 1,
    };
    Cmd->ClearColorImage(Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &range);
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
            .aspectMask = GetAspect(),
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

rc<Image> Image::Copy(rc<CommandBuffer> Cmd)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    rc<Image> Img = Image::New(Vk, ImageCreateInfo{
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
            .aspectMask = GetAspect(),
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = Img->GetAspect(),
            .layerCount = 1,
        },
        .extent = {GetEffectiveExtent().width, Extent.height, 1},
    };

    Cmd->CopyImage(Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Img->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    return Img;
}


rc<Buffer> Image::Download(rc<CommandBuffer> Cmd)
{
    assert(Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    rc<Buffer> StagingBuffer = Buffer::New(Vk, BufferCreateInfo { 
        .Size = (u32)Allocation.GetSize(), 
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
            .aspectMask = GetAspect(),
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

void Image::BlitFrom(rc<CommandBuffer> Cmd, rc<Image> Src, VkFilter Filter)
{
    Image* Dst = this;

    if(Src.get() == Dst)
    {
    	GLog.E("Image::BlitFrom: Src and Dst are the same image");
        return;
    }

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

    if (!Vk->Features.synchronization2)
    {
        VkImageBlit region = {
            .srcSubresource = {
                .aspectMask = Src->GetAspect(),
                .layerCount = 1,
            },
            .srcOffsets = {{}, {(i32)Src->Extent.width / (IsYCbCr(Src->Format) + 1), (i32)Src->Extent.height, 1}},
            .dstSubresource = {
                .aspectMask = Dst->GetAspect(),
                .layerCount = 1,
            },
            .dstOffsets = {{}, {(i32)Dst->Extent.width / (IsYCbCr(Dst->Format) + 1), (i32)Dst->Extent.height, 1}},
        };
        Cmd->BlitImage(Src->Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Dst->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1 , &region, Filter);
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
            .filter = Filter,
        };

        Cmd->BlitImage2(&blitInfo);
    }
}

void Image::CopyFrom(rc<CommandBuffer> Cmd, rc<Image> Src)
{
    if(this == Src.get())
    {
        GLog.E("Trying to copy to and copy from same resource!");
        return;
    }

    Image* Dst = this;

    assert(
        (Dst->Extent.width == Src->Extent.width && Dst->Extent.height == Src->Extent.height) ||
        Dst->Allocation.GetSize() >= Src->Allocation.GetSize()
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
            .aspectMask = Src->GetAspect(),
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = Dst->GetAspect(),
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
            .aspectMask = Src->GetAspect(),
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = Dst->GetAspect(),
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

DescriptorResourceInfo ImageView::GetDescriptorInfo(VkFilter filter) const
{
    return DescriptorResourceInfo{
        .Image = {
            .sampler     = GetDevice()->GetSampler(filter),
            .imageView   = Handle,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        }};
}

rc<ImageView> Image::GetView(VkFormat Format, VkImageUsageFlags Usage)
{ 
    Format = (Format ? Format : this->Format);
    Usage  = (Usage ? Usage : this->Usage);
    const u64 hash = (((u64)Format << 32ull) | (u64)Usage);
    auto it = Views.find(hash);
    if (it != Views.end())
    {
        return it->second;
    }
    return Views[hash] = ImageView::New(this, Format, Usage);
}


} // namespace nos::vk