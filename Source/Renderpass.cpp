// Copyright MediaZ AS. All Rights Reserved.

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

Basepass::Basepass(rc<Pipeline> PL) : DeviceChild(PL->GetDevice()), PL(PL), DescriptorPool(PL->Layout->CreatePool())
{
    if(PL->Layout->UniformSize)
    {
        UniformBuffer = vk::Buffer::New(GetDevice(), vk::BufferCreateInfo {
            .Size = PL->Layout->UniformSize,
            .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        });
    }
}

void Basepass::TransitionInput(rc<vk::CommandBuffer> Cmd, std::string const& name, rc<Image> img)
{
    auto& layout = *PL->Layout;

    if (!img && !layout.BindingsByName.contains(name))
        return;
    
    auto idx = layout[name];
    auto& dsl = layout[idx];

    if (dsl.Type->Tag == vk::SVType::Image)
    {
        VkFilter filter;
        img->Transition(Cmd, {
            .StageMask = GetStage(),
            .AccessMask = vk::Binding::MapTypeToAccess(dsl.DescriptorType),
            .Layout = vk::Binding::MapTypeToLayout(dsl.DescriptorType),
        });
        return;
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

void Basepass::BindResource(std::string const& name, std::vector<rc<Image>> res, VkFilter filter)
{
    assert(IMAGE_ARRAY == GetUniformClass(name));
    auto [binding, idx, type] = GetBindingAndType(name);
    auto& set = Bindings[idx.set];
    for (u32 i = 0; i < res.size(); ++i)
        UpdateOrInsert(set, vk::Binding(res[i], idx.binding, filter, i));
}

void Basepass::BindResource(std::string const& name, rc<Buffer> res)
{
    assert(BUFFER == GetUniformClass(name));
    auto [binding, idx, type] = GetBindingAndType(name);
    UpdateOrInsert(Bindings[idx.set], vk::Binding(res, idx.binding, 0, 0));
}

void Basepass::BindData(std::string const& name, const void* data, uint32_t sz)
{
    assert(UNIFORM == GetUniformClass(name));
    auto [binding, idx, type] = GetBindingAndType(name);

    BufferDirty = true;
    u32 baseOffset = PL->Layout->OffsetMap[((u64)idx.set<< 32ull) | idx.binding];
    u32 offset = baseOffset + idx.offset;
    UpdateOrInsert(Bindings[idx.set], vk::Binding(UniformBuffer, idx.binding, baseOffset, 0));

    auto ptr = UniformBuffer->Map() + offset;

    memset(ptr, 0, type->Size);
    memcpy(ptr, data, sz ? std::min(sz, type->Size) : type->Size);
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

void Renderpass::Exec(rc<vk::CommandBuffer> cmd,
					  rc<vk::Image> output,
					  const VertexData* Verts,
					  bool clear,
					  u32 frameNumber,
					  float deltaSeconds,
					  std::array<float, 4> clearCol)
{

    BindResources(cmd);
    Begin(cmd, output, Verts && Verts->Wireframe, clear, frameNumber, deltaSeconds, clearCol);
    Draw(cmd, Verts);
    End(cmd);

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

void Renderpass::Begin(rc<CommandBuffer> Cmd, rc<Image> SrcImage, bool wireframe, bool clear, u32 frameNumber, float deltaSeconds, std::array<float, 4> clearCol)
{
    assert(SrcImage);
    
    rc<ImageView> img = SrcImage->GetView(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    auto PL = ((GraphicsPipeline*)this->PL.get());

    VkImageView           imageView          = img->Handle;
    VkResolveModeFlagBits resolveMode        = VK_RESOLVE_MODE_NONE;
    VkImageView           resolveImageView   = 0;
    VkImageLayout         resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    if(PL->MS > 1)
    {
        rc<Image> tmp  = Image::New(SrcImage->GetDevice(), ImageCreateInfo {
            .Extent = SrcImage->GetEffectiveExtent(),
            .Format = SrcImage->GetEffectiveFormat(),
            .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Samples = (VkSampleCountFlagBits)PL->MS,
        });
        Cmd->AddDependency(tmp);
        tmp->Transition(Cmd, ImageState{
                                            .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                            .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        });

        resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        resolveImageView = imageView;
        resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        imageView = tmp->GetView(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)->Handle;
    }
    
    PL->Recreate(img->GetEffectiveFormat());

    img->Src->Transition(Cmd, ImageState{
                                           .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       });

    auto extent = img->Src->GetEffectiveExtent();
    if(!DepthBuffer || (DepthBuffer->GetEffectiveExtent().width != extent.width || 
                      DepthBuffer->GetEffectiveExtent().height != extent.height))
    {
        if (DepthBuffer)
        {
            Cmd->AddDependency(DepthBuffer);
        }
        DepthBuffer = vk::Image::New(GetDevice(), vk::ImageCreateInfo {
            .Extent = extent,
            .Format = VK_FORMAT_D32_SFLOAT,
            .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        });
    }
    
    DepthBuffer->Transition(Cmd, ImageState{
                                           .StageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                           .AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                           .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                       });

    // for (auto &set : DescriptorSets)
    // {
    //     for (auto [img, state] : set->BindStates)
    //     {
    //         img->Transition(Cmd, state);
    //     }
    //     set->Bind(Cmd);
    // }

    VkViewport viewport = {
        .width = (f32)extent.width,
        .height = (f32)extent.height,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {.extent = extent};

    Cmd->SetViewport(0, 1, &viewport);
    Cmd->SetScissor(0, 1, &scissor);
    Cmd->SetDepthTestEnable(false);
    Cmd->SetDepthWriteEnable(false);
    Cmd->SetDepthCompareOp(VK_COMPARE_OP_NEVER);

    if (!Vk->Features.dynamicRendering)
    {
        VkRenderPass rp = PL->Handles[img->GetEffectiveFormat()].rp;

        if (ImgView != img)
        {
            ImgView = img;
            if (FrameBuffer)
            {
                Vk->DestroyFramebuffer(FrameBuffer, 0);
            }
            
            VkFramebufferCreateInfo framebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = rp,
                .attachmentCount = 1,
                .pAttachments = &img->Handle,
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };

            NOSVK_ASSERT(Vk->CreateFramebuffer(&framebufferInfo, nullptr, &FrameBuffer));
        }
        VkClearValue clear = {.color = {.float32 = {0,0,0,0}}};
        VkRenderPassBeginInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = rp,
            .framebuffer = FrameBuffer,
            .renderArea = {{0, 0}, extent},
            .clearValueCount = 1,
            .pClearValues = &clear,
        };

        Cmd->BeginRenderPass(&renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    else
    {
    
        VkRenderingAttachmentInfo Attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = imageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = resolveMode,
            .resolveImageView = resolveImageView,
            .resolveImageLayout = resolveImageLayout,
            .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.color = {.float32 = {clearCol[0], clearCol[1], clearCol[2], clearCol[3]}}},
            };

        VkRenderingAttachmentInfo DepthAttachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = DepthBuffer->GetView()->Handle,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.depthStencil = { .depth = 1.f }},
        };
        
        VkRenderingInfo renderInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = extent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &Attachment,
            // .pDepthAttachment = &DepthAttachment,
        };

        Cmd->BeginRendering(&renderInfo);
    }

    auto& handle = PL->Handles[img->GetEffectiveFormat()];
    Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? handle.wpl : handle.pl);
    Cmd->AddDependency(shared_from_this());
	
    struct Constants
	{
		VkExtent2D Extent;
		u32 FrameNumber;
		float deltaSeconds;
	}
	constants = { img->Src->GetExtent(), frameNumber, deltaSeconds };
	PL->PushConstants(Cmd, constants);
}

void Renderpass::End(rc<CommandBuffer> Cmd)
{
    if (!Vk->Features.dynamicRendering)
    {
        Cmd->EndRenderPass();
    }
    else
    {
        Cmd->EndRendering();
    }

    if (UniformBuffer) // Get a new buffer so it's not overwritten by next pass
    {
        Cmd->AddDependency(UniformBuffer);
        UniformBuffer = vk::Buffer::New(GetDevice(), vk::BufferCreateInfo {
            .Size = PL->Layout->UniformSize,
                .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        });
    }
    Bindings.clear();
}

void Basepass::RefreshBuffer(rc<vk::CommandBuffer> Cmd)
{
    if(UniformBuffer && BufferDirty) // Get a new buffer so it's not overwritten by next pass
    {
        BufferDirty = false;
        Cmd->AddDependency(UniformBuffer);
        auto tmp = vk::Buffer::New(GetDevice(), vk::BufferCreateInfo {
            .Size = PL->Layout->UniformSize,
            .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        });
        memcpy(tmp->Map(), UniformBuffer->Map(), PL->Layout->UniformSize);
        UniformBuffer = tmp;
    }
}

void Basepass::UpdateDescriptorSets()
{
    DescriptorSets.clear();
    for (auto &[idx, set] : Bindings)
    {
        auto dset = DescriptorPool->AllocateSet(idx);
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
