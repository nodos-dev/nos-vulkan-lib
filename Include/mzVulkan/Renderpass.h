/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Pipeline.h"
#include <mutex>

namespace mz::vk
{

struct Buffer;


struct VertexData
{
    rc<vk::Buffer> Buffer;
    u64 VertexOffset = 0;
    u64 IndexOffset = 0;
    u64 NumIndices = 0;
    bool Wireframe  = false;
    bool DepthWrite = false;
    bool DepthTest  = false;
    VkCompareOp DepthFunc  = VK_COMPARE_OP_NEVER;
};

struct mzVulkan_API Basepass :  DeviceChild
{
    std::mutex Mutex;
    rc<Pipeline> PL;
    rc<DescriptorPool> DescriptorPool;
    std::vector<rc<DescriptorSet>> DescriptorSets;
    std::map<u32, std::set<vk::Binding>> Bindings;
    rc<Buffer> UniformBuffer;
    bool BufferDirty = false;

    Basepass(rc<Pipeline> PL);

    void Lock() { Mutex.lock(); }
    void Unlock() { Mutex.unlock(); }
    
    VkPipelineStageFlagBits2 GetStage() const
    {
        auto re = VK_PIPELINE_STAGE_2_NONE;
        if (VK_SHADER_STAGE_FRAGMENT_BIT & PL->MainShader->Stage) re |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        if (VK_SHADER_STAGE_COMPUTE_BIT & PL->MainShader->Stage) re  |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        return re;
    }

    void BindResource(std::string const& name, rc<Image> res, VkFilter filter);
    void BindResource(std::string const& name, std::vector<rc<Image>> res, VkFilter filter);
    void BindResource(std::string const& name, rc<Buffer> res);
    void BindData(std::string const& name, const void*, uint32_t sz);

    enum UniformClass
    {
        INVALID,
        IMAGE_ARRAY,
        IMAGE,
        BUFFER,
        UNIFORM,
    };
    
    auto GetBindingAndType(std::string const& name) -> std::tuple<const NamedDSLBinding*, ShaderLayout::Index, rc<SVType>>
    {
        auto it = PL->Layout->BindingsByName.find(name);
        if(it == PL->Layout->BindingsByName.end())
            return {};
        
        auto binding = &PL->Layout->DescriptorLayouts[it->second.set]->Bindings[it->second.binding];
        auto type = binding->Type;
        if (binding->Name != name)
            type = type->Members.at(name).Type;

        return {binding, it->second, type};
    }

    UniformClass GetUniformClass(std::string const& name)
    {
        auto [binding, idx, type] = GetBindingAndType(name);
        if(!binding)
            return INVALID;

        if(binding->SSBO())
            return BUFFER;
        
        if (type->Tag == vk::SVType::Image)
            return type->ArraySize ? IMAGE_ARRAY : IMAGE;
        
        return UNIFORM;
    }
    
    void TransitionInput(rc<vk::CommandBuffer> Cmd, std::string const& name, rc<Image>);

    void RefreshBuffer(rc<vk::CommandBuffer> Cmd);
    void BindResources(rc<vk::CommandBuffer> Cmd);

    void UpdateDescriptorSets();
};

struct mzVulkan_API Computepass : SharedFactory<Computepass>, Basepass
{
    Computepass(rc<ComputePipeline> PL) : Basepass(PL) {}

    void Dispatch(rc<CommandBuffer> Cmd, u32 x = 8, u32 y = 8, u32 z = 1);
};

struct mzVulkan_API Renderpass : SharedFactory<Renderpass>, Basepass
{
    VkFramebuffer FrameBuffer = 0;
    rc<Image> DepthBuffer;
    rc<ImageView> ImgView;

    Renderpass(rc<GraphicsPipeline> PL);
    Renderpass(Device* Vk, std::vector<u8> const& src);
    ~Renderpass();
    
    rc<GraphicsPipeline> GetPL() const { return ((GraphicsPipeline*)PL.get())->shared_from_this(); }
	void Begin(rc<CommandBuffer> Cmd,
			   rc<Image> Image,
			   bool wireframe = false,
			   bool clear = true,
			   u32 frameNumber = 0,
			   float deltaSeconds = .0f,
			   std::array<float, 4> clearCol = {0.0f});
    void End(rc<CommandBuffer> Cmd);
	void Exec(rc<vk::CommandBuffer> Cmd,
			  rc<vk::Image> Output,
			  const VertexData* = 0,
			  bool clear = true,
			  u32 frameNumber = 0,
			  float deltaSeconds = .0f,
			  std::array<float, 4> clearCol = {0.0f});
    void Draw(rc<vk::CommandBuffer> Cmd, const VertexData* Verts = 0);
};
}