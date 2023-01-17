// Copyright MediaZ AS. All Rights Reserved.

#include "mzVulkan/Binding.h"
#include "mzVulkan/Command.h"
#include "mzVulkan/Common.h"
#include "mzVulkan/Renderpass.h"
#include "mzVulkan/Buffer.h"
#include "vulkan/vulkan_core.h"

namespace mz::vk
{

Renderpass::Renderpass(Device* Vk, View<u8> src) : 
    Basepass(GraphicsPipeline::New(Vk, MakeShared<Shader>(Vk, src)))
{
}

Renderpass::Renderpass(rc<GraphicsPipeline> PL) : Basepass(PL)
{
}

Basepass::Basepass(Device* Vk, View<u8> src) :
    Basepass(GraphicsPipeline::New(Vk, MakeShared<Shader>(Vk, src)))
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

void Basepass::TransitionInput(rc<vk::CommandBuffer> Cmd, std::string const& name, void* data, u32 size, rc<ImageView> (ImportImage)(void*), rc<Buffer>(ImportBuffer)(void*))
{
    auto& layout = *PL->Layout;

    if (!layout.BindingsByName.contains(name))
    {
        return;
    }
    
    auto idx = layout[name];
    auto& dsl = layout[idx];

    if (dsl.Type->Tag == vk::SVType::Image)
    {
        auto view = ImportImage(data);
        auto binding = vk::Binding(view, idx.binding);
        auto info = binding.GetDescriptorInfo(dsl.DescriptorType);
        view->Src->Transition(Cmd, {
            .StageMask = GetStage(),
            .AccessMask = binding.AccessFlags,
            .Layout = info.Image.imageLayout,
        });
        return;
    }
}

void Basepass::Bind(std::string const& name, void* data, u32 size, rc<ImageView>(ImportImage)(void*), rc<Buffer>(ImportBuffer)(void*))
{
    if (!PL->Layout->BindingsByName.contains(name))
    {
        return;
    }
    
    auto idx = PL->Layout->BindingsByName[name];
    auto& binding = PL->Layout->DescriptorLayouts[idx.set]->Bindings[idx.binding];
    auto type = binding.Type;

    switch(binding.DescriptorType)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        Bindings[idx.set][idx.binding] = vk::Binding(ImportBuffer(data), idx.binding);
        return;
    default:
        break;
    }

    if (type->Tag == vk::SVType::Image)
    {
        Bindings[idx.set][idx.binding] = vk::Binding(ImportImage(data), idx.binding);
        return;
    }
    
    BufferDirty = true;
    u32 baseOffset = PL->Layout->OffsetMap[((u64)idx.set << 32ull) | idx.binding];
    u32 offset = baseOffset + idx.offset;
    Bindings[idx.set][idx.binding] = vk::Binding(UniformBuffer, idx.binding, baseOffset);
    memcpy(UniformBuffer->Map() + offset, data, size);
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

void Renderpass::Exec(rc<vk::CommandBuffer> Cmd, rc<vk::ImageView> Output, const VertexData* Verts, bool clear)
{
    BindResources(Bindings);
    Begin(Cmd, Output, Verts && Verts->Wireframe, clear);
    Draw(Cmd, Verts);
    End(Cmd);
    
    if(UniformBuffer) // Get a new buffer so it's not overwritten by next pass
    {
        Cmd->AddDependency(UniformBuffer);
        UniformBuffer = vk::Buffer::New(GetDevice(), vk::BufferCreateInfo {
            .Size = PL->Layout->UniformSize,
            .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        });
    }
}

void Basepass::BindResources(rc<vk::CommandBuffer> Cmd)
{
    BindResources(Bindings);
    for (auto &set : DescriptorSets)
    {
        set->Bind(Cmd, PL->MainShader->Stage == VK_SHADER_STAGE_FRAGMENT_BIT ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE);
    }
    RefreshBuffer(Cmd);
}

void Renderpass::Begin(rc<CommandBuffer> Cmd, rc<ImageView> Image, bool wireframe, bool clear)
{
    assert(Image);
    
    auto PL = ((GraphicsPipeline*)this->PL.get());

    PL->Recreate(Image->GetEffectiveFormat());

    Image->Src->Transition(Cmd, ImageState{
                                           .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       });

    auto extent = Image->Src->GetEffectiveExtent();
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

    for (auto &set : DescriptorSets)
    {
        for (auto [img, state] : set->BindStates)
        {
            img->Transition(Cmd, state);
        }
        set->Bind(Cmd);
    }

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

    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        VkRenderPass rp = PL->Handles[Image->GetEffectiveFormat()].rp;

        if (m_ImageView != Image)
        {
            m_ImageView = Image;
            if (FrameBuffer)
            {
                Vk->DestroyFramebuffer(FrameBuffer, 0);
            }
            
            VkFramebufferCreateInfo framebufferInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = rp,
                .attachmentCount = 1,
                .pAttachments = &Image->Handle,
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };

            MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateFramebuffer(&framebufferInfo, nullptr, &FrameBuffer));
        }

        VkRenderPassBeginInfo renderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = rp,
            .framebuffer = FrameBuffer,
            .renderArea = {{0, 0}, extent},
        };

        Cmd->BeginRenderPass(&renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    else
    {
        VkRenderingAttachmentInfo Attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = Image->Handle,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            // .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = {.float32 = {0,0,0,0}}},
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
    
    auto& handle = PL->Handles[Image->GetEffectiveFormat()];
    Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? handle.wpl : handle.pl);
    PL->PushConstants(Cmd, Image->Src->GetExtent());
    Cmd->AddDependency(shared_from_this());
}

void Renderpass::End(rc<CommandBuffer> Cmd)
{
    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
    {
        Cmd->EndRenderPass();
    }
    else
    {
        Cmd->EndRendering();
    }
}

void Basepass::BindResources(std::map<u32, std::vector<Binding>> const &bindings)
{
    DescriptorSets.clear();
    for (auto &[idx, set] : bindings)
    {
        DescriptorSets.push_back(DescriptorPool->AllocateSet(idx)->Update(set));
    }
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

void Basepass::BindResources(std::map<u32, std::map<u32, Binding>> const &bindings)
{
    DescriptorSets.clear();
    for (auto &[idx, set] : bindings)
    {
        DescriptorSets.push_back(DescriptorPool->AllocateSet(idx)->Update(set));
    }
}

bool Basepass::BindResources(std::unordered_map<std::string, Binding::Type> const &resources)
{
    std::map<u32, std::map<u32, Binding>> Bindings;

    for (auto &[name, res] : resources)
    {
        auto it = PL->Layout->BindingsByName.find(name);
        if (it == PL->Layout->BindingsByName.end())
        {
            return false;
        }
        Bindings[it->second.set][it->second.binding] = Binding(res, it->second.binding);
    }

    BindResources(Bindings);

    return true;
}

Renderpass::~Renderpass()
{
    if (Vk->FallbackOptions.mzDynamicRenderingFallback)
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
