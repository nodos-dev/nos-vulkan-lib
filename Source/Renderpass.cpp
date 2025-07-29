// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "nosVulkan/Binding.h"
#include "nosVulkan/Command.h"
#include "nosVulkan/Common.h"
#include "nosVulkan/Renderpass.h"
#include "nosVulkan/Buffer.h"
#include "vulkan/vulkan_core.h"

namespace nos::vk
{

Renderpass::Renderpass(Device* Vk, std::vector<u8> const& src) : 
    Basepass(GraphicsPipeline::New(Vk, MakeShared<Shader>(Vk, src)))
{
}

Renderpass::Renderpass(rc<GraphicsPipeline> PL) : Basepass(PL)
{
}

rc<Buffer> Basepass::CreateUniformSizedBuffer()
{
	// TODO: Use the resource pool for uniform buffers
	auto bufRes = Buffer::CreateRelaxed(Vk, vk::BufferCreateRequest{
		.Resource = {
			.ExternalMemory = VkExternalMemoryHandleTypeFlags(0),
		},
		.Size = PL->Layout->UniformSize,
		.Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.MemProps = {.Mapped = true, .Download = false}
	});
    if(auto err = bufRes.Error())
	{
		GLog.E("Basepass::CreateUniformSizedBuffer: Failed to create buffer: %s", err->c_str());
		return nullptr;
	}
	return *bufRes.Get();
}

rc<Buffer> Basepass::CreateStorageBuffer(u64 size) {
	auto bufRes = Buffer::CreateRelaxed(Vk, vk::BufferCreateRequest{
		.Resource = {
			.ExternalMemory = VkExternalMemoryHandleTypeFlags(0),
		},
		.Size = size,
		.Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.MemProps = {.Mapped = true, .Download = false}
	});
	if (auto err = bufRes.Error())
	{
		GLog.E("Basepass::CreateStorageBuffer: Failed to create buffer: %s", err->c_str());
		return nullptr;
	}
    return *bufRes.Get();
}

Basepass::Basepass(rc<Pipeline> PL) : DeviceChild(PL->GetDevice()), PL(PL), PassDescriptorPool(PL->Layout->CreatePool())
{
    if(PL->Layout->UniformSize)
		UniformBuffer = CreateUniformSizedBuffer();

	for (auto size : PL->Layout->SizeMap)
	{
		StorageBuffers[size.first] = { CreateStorageBuffer(size.second), false };
	}
}

void Basepass::TransitionInput(rc<vk::CommandBuffer> Cmd, std::string const& name, rc<Image> img)
{
	auto& layout = *PL->Layout;

	if (!img || !layout.BindingsByName.contains(name))
		return;
    
	auto idx = layout[name];
	auto& dsl = layout[idx];

	if (dsl.Type->Tag == vk::SVType::Image)
	{
		ImageState state = {
			.StageMask = GetStage(),
			.AccessMask = vk::Binding::MapTypeToAccess(dsl.DescriptorType), // TODO: Look into access flags to optimize image memory barriers for VK_DESCRIPTOR_TYPE_STORAGE_IMAGEs
			.Layout = vk::Binding::MapTypeToLayout(dsl.DescriptorType),
			.QueueFamilyIndex = Cmd->Pool->PoolQueue->FamilyIndex
		};
		img->Transition(Cmd, state);
	}
}

void Basepass::TransitionInput(rc<vk::CommandBuffer> cmd, std::string const& name, rc<Buffer> buf)
{
	auto& layout = *PL->Layout;

	if (!buf || !layout.BindingsByName.contains(name))
		return;
    
	auto idx = layout[name];
	auto& dsl = layout[idx];
	
	if (dsl.Type->Tag == vk::SVType::Struct)
	{
		auto access = dsl.Access;
		vk::BufferMemoryState newState {
			.StageMask = GetStage(),
			.AccessMask = 0
		};
		if (vk::AccessFlagRead & access)
			newState.AccessMask |= VK_ACCESS_2_MEMORY_READ_BIT;
		if (vk::AccessFlagWrite & access)
			newState.AccessMask |= VK_ACCESS_2_MEMORY_WRITE_BIT;
		buf->Transition(cmd, newState, 0, buf->Size);
	}
}

static void UpdateOrInsert(std::set<vk::Binding>& bindings, vk::Binding&& binding)
{
    auto it = bindings.find(binding);
    if(it != bindings.end())
        bindings.erase(it);
    bindings.insert(binding);
}

void Basepass::BindResource(std::string const& name, rc<Image> res, VkFilter filter)
{
    assert(IMAGE == GetUniformClass(name));
    auto [binding, idx, type] = GetBindingAndType(name);
    UpdateOrInsert(Bindings[idx.set], vk::Binding(res, idx.binding, filter, 0));
}

void Basepass::BindResource(std::string const& name, std::vector<std::pair<rc<Image>, VkFilter>> res)
{
    assert(IMAGE_ARRAY == GetUniformClass(name));
    auto [binding, idx, type] = GetBindingAndType(name);
    auto& set = Bindings[idx.set];
    for (u32 i = 0; i < res.size(); ++i)
        UpdateOrInsert(set, vk::Binding(res[i].first, idx.binding, res[i].second, i));
}

void Basepass::BindResource(std::string const& name, rc<Buffer> res)
{
    assert(BUFFER == GetUniformClass(name));
    auto [binding, idx, type] = GetBindingAndType(name);
    UpdateOrInsert(Bindings[idx.set], vk::Binding(res, idx.binding, 0, 0));
}

void Basepass::BindData(std::string const& name, const void* data, uint32_t sz)
{
    auto uniformClass = GetUniformClass(name);
    assert(UNIFORM == uniformClass || BUFFER == uniformClass);
    auto [binding, idx, type] = GetBindingAndType(name);

	rc<nos::vk::Buffer> buffer = nullptr;
    if (uniformClass == UNIFORM) {
        UniformBufferDirty = true;
        buffer = UniformBuffer;
    }
	else if (uniformClass == BUFFER) {
		buffer = StorageBuffers[(u64(idx.set) << 32ull) | idx.binding].first;
		StorageBuffers[(u64(idx.set) << 32ull) | idx.binding].second = true;
	}

    u32 baseOffset = PL->Layout->OffsetMap[((u64)idx.set << 32ull) | idx.binding];
    u32 offset = baseOffset + idx.offset;
    uint32_t copySize = sz ? std::min(sz, type->Size) : type->Size;
    uint32_t clearSize = std::max(4u, sz);


	// If the data is a VLA, copy all of the passed data
    u64 structSizeWithoutVLA = binding->Type->Size;
    if (uniformClass == BUFFER && offset == structSizeWithoutVLA) {
        copySize = sz;
    }

    auto ptr = buffer->Map() + offset;

    UpdateOrInsert(Bindings[idx.set], vk::Binding(buffer, idx.binding, baseOffset, 0));

    memset(ptr, 0, clearSize);
    memcpy(ptr, data, copySize);
}

void Renderpass::Draw(rc<vk::CommandBuffer> Cmd, const VertexData* Verts)
{
    if(Verts)
    {
        Cmd->SetDepthWriteEnable(Verts->DepthWrite);
        Cmd->SetDepthTestEnable(Verts->DepthTest);
        Cmd->SetDepthCompareOp(Verts->DepthFunc);
        Cmd->BindVertexBuffers(0, 1, &Verts->Buffer->Handle, &Verts->VertexOffset);
        Cmd->BindIndexBuffer(Verts->Buffer->Handle, Verts->IndexOffset, VK_INDEX_TYPE_UINT32);
        Cmd->DrawIndexed(Verts->NumIndices, 1, 0, 0, 0);
    }
    else
    {
        Cmd->Draw(6, 1, 0, 0);
    }
}

std::optional<std::string> Renderpass::Exec(rc<vk::CommandBuffer> cmd, const ExecPassInfo& info)
{
    if (!Vk->Features.dynamicRendering)
    {
        return "Dynamic rendering is not supported on this device.";
    }
    BindResources(cmd);
    if (auto err = Begin(cmd, info.BeginInfo))
        return err;
    Draw(cmd, info.VtxData);
    End(cmd);
    return std::nullopt;
}

void Basepass::BindResources(rc<vk::CommandBuffer> Cmd)
{
    UpdateDescriptorSets();
    for (auto &set : DescriptorSets)
    {
        set->Bind(Cmd, PL->MainShader->Stage == VK_SHADER_STAGE_FRAGMENT_BIT ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE);
    }
    DescriptorSets.clear();
    RefreshBuffer(Cmd);
}

std::optional<std::string> Renderpass::Begin(rc<CommandBuffer> cmd, const BeginPassInfo& info)
{
    if(!info.OutImages.empty())
		return "No output image provided";
    
    auto PL = ((GraphicsPipeline*)this->PL.get());


    
    std::vector<rc<ImageView>> images;
    std::vector<VkImageView> rawViews;

    
    for(auto& img : info.OutImages)
    {
        auto view = img->GetView(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        images.push_back(img->GetView(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
        rawViews.push_back(view->Handle);
    }

    std::vector<VkImageView> resolveViews;
    std::vector<rc<Image>> localMsBuffers;
    
    if(PL->MS > 1)
    {
        resolveViews = std::move(rawViews);
        for(auto& img : info.OutImages)
        {
            auto relaxedRequest = Image::TryGetRelaxedSuitableCreateRequest(Vk, ImageCreateRequest{
                .Resource = {
                    .ExternalMemory = VkExternalMemoryHandleTypeFlags(0),
                },
                .Extent = img->GetEffectiveExtent(),
                .Format = img->GetEffectiveFormat(),
                .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .Samples = (VkSampleCountFlagBits)PL->MS
            });
            if (auto err = relaxedRequest.Error())
                return "Failed to create temporary multisample resource: " + *err;
            auto result = GetDevice()->ResourcePools.Image->Get(*relaxedRequest.Get(), "Temporary Multisample Resource");
            if(auto err = result.Error())
                return "Failed to create temporary multisample resource: " + *err;

            auto tex = *result.Get();
            tex->Transition(cmd, ImageState{
                                                .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                            });
            localMsBuffers.push_back(tex);
            rawViews.push_back(tex->GetView(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)->Handle);
        }
    }
    std::vector<VkFormat> formats;
    for(auto& img : info.OutImages)
        formats.push_back(img->GetEffectiveFormat());
    
    PL->Recreate(&formats[0], formats.size());
    
    for(auto& img : images)
        img->Src->Transition(cmd, ImageState{
                                            .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        });

    auto extent = images[0]->Src->GetEffectiveExtent();
	rc<Image> optionalDepthBuffer = info.DepthAttachment ? info.DepthAttachment->DepthBuffer : nullptr;
	bool depthClear = true;
	float depthClearVal = 1.0f;
	if (optionalDepthBuffer)
	{
		depthClear = info.DepthAttachment->Clear;
		depthClearVal = info.DepthAttachment->ClearValue;
		optionalDepthBuffer->Transition(cmd, ImageState{
												.StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
												.AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
												.Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			});
    }

    VkViewport viewport = {
        .width = (f32)extent.width,
        .height = (f32)extent.height,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {.extent = extent};

    cmd->SetViewport(0, 1, &viewport);
    cmd->SetScissor(0, 1, &scissor);
    cmd->SetDepthTestEnable(false);
    cmd->SetDepthWriteEnable(false);
    cmd->SetDepthCompareOp(VK_COMPARE_OP_NEVER);

    auto data = PL->GetPipelineData(&formats[0], formats.size());
    if (!Vk->Features.dynamicRendering)
    {
        if (Views != images)
        {
            Views = images;
            if (FrameBuffer)
            {
                Vk->DestroyFramebuffer(FrameBuffer, 0);
            }
            
            VkFramebufferCreateInfo framebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = data.rp,
                .attachmentCount = (u32)rawViews.size(),
                .pAttachments = rawViews.data(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };

            NOSVK_ASSERT(Vk->CreateFramebuffer(&framebufferInfo, nullptr, &FrameBuffer));
        }
        VkClearValue clear = {.color = {.float32 = {0,0,0,0}}};
        VkRenderPassBeginInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = data.rp,
            .framebuffer = FrameBuffer,
            .renderArea = {{0, 0}, extent},
            .clearValueCount = 1,
            .pClearValues = &clear,
        };

        cmd->BeginRenderPass(&renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    else
    {
        std::vector<VkRenderingAttachmentInfo> attachments;
        for(u32 i = 0; i < rawViews.size(); ++i)
        {
            VkRenderingAttachmentInfo att = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = rawViews[i],
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = info.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue =
                    {.color = {.float32 = {info.ClearCol[0], info.ClearCol[1], info.ClearCol[2], info.ClearCol[3]}}},
            };
            if(PL->MS > 1)
            {
                att.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
                att.resolveImageView = resolveViews[i];
                att.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
            attachments.push_back(att);
        }

        VkRenderingAttachmentInfo DepthAttachment;

        if (optionalDepthBuffer)
        {
            DepthAttachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = optionalDepthBuffer->GetView()->Handle,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .loadOp = depthClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {.depthStencil = { .depth = depthClearVal }},
            };
        }
        
        VkRenderingInfo renderInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = extent},
            .layerCount = 1,
            .colorAttachmentCount = (u32)attachments.size(),
            .pColorAttachments = attachments.data(),
            .pDepthAttachment = optionalDepthBuffer ? &DepthAttachment : nullptr,
        };
        cmd->BeginRendering(&renderInfo);
    }

    cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, info.Wireframe ? data.wpl : data.pl);
    cmd->AddDependency(shared_from_this());
	if (Vk->Features.dynamicRendering)
		cmd->SetCullMode(info.CullMode);
	
    struct Constants
	{
		VkExtent2D Extent;
		u64 FrameNumber;
	}
	constants = { extent, info.FrameNumber };
	PL->PushConstants(cmd, constants);
	if (!localMsBuffers.empty())
    {
        auto dev = GetDevice();
        for(auto buf : localMsBuffers)
            dev->ResourcePools.Image->Release(uint64_t(buf->Handle));
    }
    return std::nullopt;
}

void Renderpass::End(rc<CommandBuffer> Cmd)
{
    if (!Vk->Features.dynamicRendering)
        Cmd->EndRenderPass();
    else
        Cmd->EndRendering();

    Bindings.clear();
}

void Basepass::RefreshBuffer(rc<vk::CommandBuffer> Cmd)
{
    // Get a new buffer so it's not overwritten by next pass
    if(UniformBuffer && UniformBufferDirty)
    {
        UniformBufferDirty = false;
        Cmd->AddDependency(UniformBuffer);
        auto tmp = CreateUniformSizedBuffer();
        memcpy(tmp->Map(), UniformBuffer->Map(), PL->Layout->UniformSize);
        UniformBuffer = tmp;
    }
}

void Basepass::UpdateDescriptorSets()
{
    DescriptorSets.clear();
    for (auto &[idx, set] : Bindings)
    {
        auto dset = PassDescriptorPool->AllocateSet(idx);
        dset->Update(set);
        DescriptorSets.push_back(std::move(dset));
    }
}

Renderpass::~Renderpass()
{
    if (!Vk->Features.dynamicRendering)
    {
        if (FrameBuffer)
        {
            Vk->DestroyFramebuffer(FrameBuffer, 0);
        }
    }
}

void Computepass::Dispatch(rc<CommandBuffer> Cmd, u32 x, u32 y, u32 z)
{
    auto PL = (ComputePipeline*)this->PL.get();
    Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, PL->Handle);
    Cmd->AddDependency(shared_from_this());
    Cmd->Dispatch(x, y, z);
}

}
