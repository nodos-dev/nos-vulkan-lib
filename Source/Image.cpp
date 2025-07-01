// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "vulkan/vulkan_core.h"
#include <nosVulkan/Image.h>
#include <nosVulkan/Device.h>
#include <nosVulkan/Command.h>
#include <nosVulkan/Buffer.h>

namespace nos::vk
{
static VkImageType GetImageType()
{
	return VK_IMAGE_TYPE_2D; // Temporary fix for color nodes.
	// return (1 >= Extent.height) ? VK_IMAGE_TYPE_1D : VK_IMAGE_TYPE_2D;
}

static VkFormat GetEffectiveFormat(VkFormat format)
{
	return IsYCbCr(format) ? VK_FORMAT_R8G8B8A8_UNORM : format;
}

static VkExtent2D GetEffectiveExtent(VkExtent2D extent, VkFormat format)
{
	return { extent.width / (1 + IsYCbCr(format)), extent.height};
}

Image::~Image()
{
	Views.clear();
	if (AllocationInfo)
	{
		if (AllocationInfo->Imported)
		{
			Vk->DestroyImage(Handle, 0);
			Vk->OnMemoryFreed(AllocationInfo->GetMemory());
			Vk->FreeMemory(AllocationInfo->GetMemory(), 0);
		}
		else if (AllocationInfo->Handle)
			vmaDestroyImage(Vk->Allocator, Handle, AllocationInfo->Handle);
	}
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

ImageCreationInfos::ImageCreationInfos(ImageCreationInfos&& o) noexcept
{
	this->MemoryTypeIndex = o.MemoryTypeIndex;
	this->ExtMemHandleType = o.ExtMemHandleType;
	this->ImgCreateInfo = o.ImgCreateInfo;
	this->AllocCreateInfo = o.AllocCreateInfo;
	this->MemProps = o.MemProps;
	this->ExtMemCreateInfo = o.ExtMemCreateInfo;
	if (ImgCreateInfo.pNext)
		this->ImgCreateInfo.pNext = &ExtMemCreateInfo;
}

Result<ImageCreationInfos> CalculateImageCreationInfos(vk::Device* device, ImageCreateRequest const& request)
{
	ImageCreationInfos ret{};
	auto& extMemHandleType = (ret.ExtMemHandleType = request.Resource.GetExportHandleTypes());
	if (auto importInfo = request.Resource.GetImportInfo())
		extMemHandleType = importInfo->HandleType;
	auto& extMemCreateInfo = ret.ExtMemCreateInfo;
	auto& imageCreateInfo = ret.ImgCreateInfo;
	auto& allocCreateInfo = ret.AllocCreateInfo;

	VkPhysicalDeviceExternalImageFormatInfo extImageFormatInfo = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		.handleType = VkExternalMemoryHandleTypeFlagBits(extMemHandleType),
	};

	VkPhysicalDeviceImageFormatInfo2 formatInfo = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		.pNext = extMemHandleType ? &extImageFormatInfo : nullptr,
		.format = GetEffectiveFormat(request.Format),
		.type = GetImageType(),
		.tiling = request.Tiling,
		.usage = request.Usage,
		.flags = request.Flags,
	};

	VkExternalImageFormatProperties extProps = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
	};

	VkImageFormatProperties2 props
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
		.pNext = extMemHandleType ? &extProps : nullptr,
	};

	auto res = vkGetPhysicalDeviceImageFormatProperties2(device->PhysicalDevice, &formatInfo, &props);
	if (NOS_VULKAN_FAILED(res))
		return "Failed to get image format properties.";

	if (extMemHandleType)
	{
		if (request.Resource.IsImported())
		{
			if(!(extProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
				return "External memory not importable.";
		}
		else if (!(extProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT))
			return "External memory not exportable.";
		if (!(extProps.externalMemoryProperties.compatibleHandleTypes & extMemHandleType))
			return "External memory handle type not supported.";
	}

	extMemCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		.handleTypes = extMemHandleType,
	};

	imageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = extMemHandleType ? &extMemCreateInfo : 0,
		.flags = request.Flags,
		.imageType = GetImageType(),
		.format = GetEffectiveFormat(request.Format),
		.extent = {GetEffectiveExtent(request.Extent, request.Format).width, request.Extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = request.Samples,
		.tiling = request.Tiling,
		.usage = request.Usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	ret.MemProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (request.Resource.IsImported())
	{
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		return ret;
	}
	else // Exported
	{
		allocCreateInfo = {.usage = VMA_MEMORY_USAGE_AUTO, .requiredFlags = ret.MemProps};

		uint32_t memoryTypeIndex = UINT32_MAX;
		auto res = vmaFindMemoryTypeIndexForImageInfo(device->Allocator, &imageCreateInfo, &allocCreateInfo, &memoryTypeIndex);
		if (res != VK_SUCCESS)
		{
			if (extMemHandleType)
				return "Failed to find memory type index for image, maybe try to create it without exporting.";
			return "Failed to find memory type index for image.";
		}
		return ret;
	}
}

ImageCreateRequest GetTempImageCreateRequest(VkExtent2D extent, VkFormat format)
{
	return {
		.Resource = {
			.Temporary = true,
			.ExternalMemory = VkExternalMemoryHandleTypeFlags(0),
		},
		.Extent = {extent.width, extent.height},
		.Format = (VkFormat)format,
		.Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	};
}

Image::Image(Device* vk,
			 VkImage img,
			 VkExtent2D extent,
			 VkFormat format,
			 VkImageUsageFlags usage,
			 ImageState state,
			 std::optional<Allocation> allocation,
			 VkDeviceSize size)
	: ResourceBase(vk), Extent(extent), Format(format), Usage(usage), State(state)
{
	AllocationInfo = std::move(allocation);
	Size = size;
	Handle = img;
	VkMemoryRequirements memReq = {};
#ifndef NDEBUG
	Vk->GetImageMemoryRequirements(Handle, &memReq);
	assert(Size == memReq.size);
#endif
}

void Image::Transition(
	rc<CommandBuffer> curCmd,
	ImageState Dst)
{
	// Dst.AccessMask = 0;
	// Dst.StageMask  = 0;
	Dst.QueueFamilyIndex = curCmd->Pool->PoolQueue->FamilyIndex;
	if (auto prevCmd = State.PreviousCmd.lock(); prevCmd && prevCmd->Pool->PoolQueue->FamilyIndex != Dst.QueueFamilyIndex)
	{
		// Previous command buffer is in a different queue family. Add wait semaphore to current command buffer.
		curCmd->WaitGroup[prevCmd->FinishedSem->Handle] = {prevCmd->SubmitCount, 0};
	}
	if (!Vk->Features.synchronization2)
		ImageLayoutTransition(Handle, curCmd, State, Dst, GetAspect());
	else
		ImageLayoutTransition2(Handle, curCmd, State, Dst, GetAspect());
	State = Dst;
	State.PreviousCmd = curCmd;
	curCmd->Callbacks.push_back([this, cmd=curCmd.get()] {
		if (State.PreviousCmd.lock().get() == cmd)
			State.PreviousCmd.reset();
	});
	curCmd->AddDependency(shared_from_this());
}

void Image::Clear(rc<CommandBuffer> Cmd, VkClearColorValue value)
{
	assert(Usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	Transition(Cmd, ImageState{
						.StageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
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

	auto imgRes = Create(Vk, ImageCreateRequest{
													.Extent = Extent,
													.Format = Format,
													.Usage  = Usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
												});

	if (auto err = imgRes.Error())
	{
		GLog.E("Image::Copy: Failed to create image: {}", err->c_str());
		return nullptr;
	}
	auto& img = *imgRes.Get();
	img->Transition(Cmd, ImageState{
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
			.aspectMask = img->GetAspect(),
			.layerCount = 1,
		},
		.extent = {GetEffectiveExtent().width, Extent.height, 1},
	};

	Cmd->CopyImage(Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img->Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	return img;
}


rc<Buffer> Image::Download(rc<CommandBuffer> Cmd)
{
	assert(Usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	auto stagingBufferRes = Buffer::Create(Vk, BufferCreateRequest { 
		.Size = (u32)Size, 
		.Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
	});

	if(auto err = stagingBufferRes.Error())
	{
		GLog.E("Image::Download: Failed to create staging buffer");
		return nullptr;
	}
	
	Download(Cmd, *stagingBufferRes.Get());
	return *stagingBufferRes.Get();
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
		Dst->Size >= Src->Size
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

VkExtent2D Image::GetEffectiveExtent() const
{
	return vk::GetEffectiveExtent(Extent, Format);
}
	
VkFormat Image::GetEffectiveFormat() const
{
	return vk::GetEffectiveFormat(Format);
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

VkImageType Image::GetImageType() const
{
	return vk::GetImageType();
}
Result<rc<Image>> Image::Create(Device* Vk, ImageCreateRequest const& createInfo, VkResult* outVkRes)
{
	VkResult res{};
	if (!outVkRes)
		outVkRes = &res;
	// Might return error without vulkan failure
	*outVkRes = VK_SUCCESS;

	auto icInfosRes = CalculateImageCreationInfos(Vk, createInfo);
	if (auto err = icInfosRes.Error())
		return std::move(*err);
	ImageState state{
		.Layout = icInfosRes.Get()->ImgCreateInfo.initialLayout,
	};

	auto& icInfos = *icInfosRes.Get();

	auto allocationInfo = vk::Allocation{};

	VkImage handle{};
	if (auto* imported = createInfo.Resource.GetImportInfo())
	{
		if(NOS_VULKAN_FAILED(*outVkRes = Vk->CreateImage(&icInfos.ImgCreateInfo, 0, &handle)))
			return "Error while creating imported image.";
		if (NOS_VULKAN_FAILED(*outVkRes = allocationInfo.Import(Vk, handle, *imported, icInfos.MemProps)))
		{
			Vk->DestroyImage(handle, 0);
			return "Error while importing image memory.";
		}
	}
	else // Exported
	{
		if (icInfos.MemoryTypeIndex != UINT32_MAX && createInfo.Resource.Temporary)
		{
			auto it = Vk->TempMemoryPools.find(icInfos.MemoryTypeIndex);
			if (it != Vk->TempMemoryPools.end())
				icInfos.AllocCreateInfo.pool = it->second;
		}
		if (NOS_VULKAN_FAILED(*outVkRes = vmaCreateImage(Vk->Allocator, &icInfos.ImgCreateInfo, &icInfos.AllocCreateInfo, &handle, &allocationInfo.Handle, &allocationInfo.Info)))
		{
			if(handle)
				Vk->DestroyImage(handle, 0);
			return "Error while creating image.";
		}
	}

#ifndef NDEBUG
	VkMemoryRequirements memReq = {};
	Vk->GetImageMemoryRequirements(handle, &memReq);
	assert(memReq.size == allocationInfo.GetSize());
#endif

	if (icInfos.ExtMemHandleType)
		if (NOS_VULKAN_FAILED(*outVkRes = allocationInfo.SetExternalMemoryHandleType(Vk, icInfos.ExtMemHandleType)))
		{
			assert(!createInfo.Resource.GetImportInfo());
			Vk->DestroyImage(handle, 0);
			return "Error while setting external memory handle type.";
		}

	return FromExisting(Vk, handle, createInfo.Extent, createInfo.Format, createInfo.Usage, state, std::move(allocationInfo), allocationInfo.GetSize());
}
rc<Image> Image::FromExisting(Device* Vk,
							  VkImage img,
							  VkExtent2D extent,
							  VkFormat format,
							  VkImageUsageFlags usage,
							  ImageState state,
							  std::optional<Allocation> allocation,
							  VkDeviceSize size)
{
	return New(Vk, img, extent, format, usage, state, std::move(allocation), size);
}
Result<ImageCreateRequest> Image::TryGetRelaxedSuitableCreateRequest(Device* Vk, ImageCreateRequest const& info)
{
	auto request = info;

	VkFormatProperties formatProps{};
	vkGetPhysicalDeviceFormatProperties(Vk->PhysicalDevice, request.Format, &formatProps);

	constexpr auto getSupportedUsages = [](VkFormatFeatureFlags features, VkImageUsageFlags usage) -> VkImageUsageFlags {
		auto ret = usage;
		if (!(features & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT))
			ret &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		if (!(features & VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT))
			ret &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (!(features & VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT))
			ret &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (!(features & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT))
			ret &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		if (!(features & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT))
			ret &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (!(features & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT))
			ret &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		return ret;
		};

	auto tilingFeatures = request.Tiling == VK_IMAGE_TILING_LINEAR ? formatProps.linearTilingFeatures : formatProps.optimalTilingFeatures;

	if (auto curUsage = getSupportedUsages(tilingFeatures, request.Usage); curUsage != request.Usage)
	{
		GLog.W("CreateImage: Requested usage not supported, trying to relax.");
		tilingFeatures = request.Tiling == VK_IMAGE_TILING_LINEAR ? formatProps.optimalTilingFeatures : formatProps.linearTilingFeatures;
		auto otherUsage = getSupportedUsages(tilingFeatures, request.Usage);
		if (std::popcount(curUsage) < std::popcount(otherUsage))
		{
			GLog.W("CreateImage: Better relaxed usage found with different tiling.");
			request.Usage = otherUsage;
			request.Tiling = request.Tiling == VK_IMAGE_TILING_LINEAR ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
		}
		else
			request.Usage = curUsage;
	}

	if (auto res = CalculateImageCreationInfos(Vk, request); auto err = res.Error())
	{
		if(request.Resource.GetImportInfo())
			return *err;
		if(!request.Resource.GetExportHandleTypes())
			return *err;
		GLog.W("CreateImage: Failed to calculate image creation info(%s), trying without exporting memory.", err->c_str());
		request.Resource.ExternalMemory = VkExternalMemoryHandleTypeFlags(0);
		if (auto res = CalculateImageCreationInfos(Vk, request); auto err = res.Error())
			return *err;
	}
return request;
}
Result<rc<Image>> Image::CreateRelaxed(Device* Vk, ImageCreateRequest const& createInfo, VkResult* vkRes)
{
	if(vkRes)
		*vkRes = VK_SUCCESS;
	if (auto res = TryGetRelaxedSuitableCreateRequest(Vk, createInfo); auto val = res.Get())
		return Create(Vk, *val, vkRes);
	else
		return *res.Error();
}
} // namespace nos::vk