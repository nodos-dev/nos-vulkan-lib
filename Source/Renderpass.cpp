#include "mzVulkan/Common.h"
#include "mzVulkan/Renderpass.h"
#include "mzVulkan/Buffer.h"


namespace mz::vk
{

Renderpass::Renderpass(Device* Vk, View<u8> src) : 
    Renderpass(Pipeline::New(Vk, MakeShared<Shader>(Vk, src)))
{
    
}

Renderpass::Renderpass(rc<Pipeline> PL) : DeviceChild(PL->GetDevice()), PL(PL), DescriptorPool(PL->Layout->CreatePool())
{
    if(PL->Layout->UniformSize)
    {
        UniformBuffer = vk::Buffer::New(GetDevice(), vk::BufferCreateInfo {
            .Size = PL->Layout->UniformSize,
            .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        });
    }
}


void Renderpass::Bind(std::string const& name, void* data, u32 size, rc<ImageView> (Import)(void*))
{
    if (!PL->Layout->BindingsByName.contains(name))
    {
        return;
    }
    
    auto idx = PL->Layout->BindingsByName[name];
    u32 baseOffset = PL->Layout->OffsetMap[((u64)idx.set << 32ull) | idx.binding];
    u32 offset = baseOffset + idx.offset;
    auto type = PL->Layout->DescriptorLayouts[idx.set]->Bindings[idx.binding].Type;

    if (type->Tag == vk::SVType::Image)
    {
        Bindings[idx.set][idx.binding] = vk::Binding(Import(data), idx.binding);
        return;
    }

    Bindings[idx.set][idx.binding] = vk::Binding(UniformBuffer, idx.binding, baseOffset);
    memcpy(UniformBuffer->Map() + offset, data, size);
}

void Renderpass::Exec(rc<vk::CommandBuffer> Cmd, rc<vk::ImageView> Output, const VertexData* Verts)
{
    BindResources(Bindings);
    Output->Src->Lock();
    for(auto set : DescriptorSets)
    {
        for(auto& [img, _] : set->BindStates)
        {
            img->Lock();
        }
    }
    
    Begin(Cmd, Output, Verts && Verts->Wireframe);
    if(Verts)
    {
        Cmd->BindVertexBuffers(0, 1, &Verts->Buffer->Handle, &Verts->VertexOffset);
        Cmd->BindIndexBuffer(Verts->Buffer->Handle, Verts->IndexOffset, VK_INDEX_TYPE_UINT32);
        Cmd->DrawIndexed(Verts->NumIndices, 1, 0, 0, 0);
    }
    else
    {
        Cmd->Draw(6, 1, 0, 0);
    }
    End(Cmd);
    
    if(UniformBuffer) // Get a new buffer so it's not overwritten by next pass
    {
        UniformBuffer = vk::Buffer::New(GetDevice(), vk::BufferCreateInfo {
            .Size = PL->Layout->UniformSize,
            .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        });
    }

    Output->Src->Unlock();
    for(auto set : DescriptorSets)
    {
        for(auto& [img, _] : set->BindStates)
        {
            img->Unlock();
        }
    }
}

void Renderpass::Begin(rc<CommandBuffer> Cmd, rc<ImageView> Image, bool wireframe)
{
    assert(Image);

    PL->Recreate(Image->GetEffectiveFormat());

    Image->Src->Transition(Cmd, ImageState{
                                           .StageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           .AccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                           .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       });

    for (auto &set : DescriptorSets)
    {
        set->Bind(Cmd);
    }

    VkExtent2D extent = Image->Src->GetEffectiveExtent();

    VkViewport viewport = {
        .width = (f32)extent.width,
        .height = (f32)extent.height,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {.extent = extent};

    Cmd->SetViewport(0, 1, &viewport);
    Cmd->SetScissor(0, 1, &scissor);
    
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
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };

        VkRenderingInfo renderInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = extent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &Attachment,
        };

        Cmd->BeginRendering(&renderInfo);
    }
    auto& handle = PL->Handles[Image->GetEffectiveFormat()];
    Cmd->BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? handle.wpl : handle.pl);

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

void Renderpass::BindResources(std::map<u32, std::vector<Binding>> const &bindings)
{
    DescriptorSets.clear();
    for (auto &[idx, set] : bindings)
    {
        DescriptorSets.push_back(DescriptorPool->AllocateSet(idx)->Update(set));
    }
}

void Renderpass::BindResources(std::map<u32, std::map<u32, Binding>> const &bindings)
{
    DescriptorSets.clear();
    for (auto &[idx, set] : bindings)
    {
        DescriptorSets.push_back(DescriptorPool->AllocateSet(idx)->Update(set));
    }
}

bool Renderpass::BindResources(std::unordered_map<std::string, Binding::Type> const &resources)
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

}