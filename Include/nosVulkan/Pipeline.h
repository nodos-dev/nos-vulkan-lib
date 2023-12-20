/*
 * Copyright MediaZ AS. All Rights Reserved.
 */


#pragma once

#include "Layout.h"
#include "Shader.h"
#include "Command.h"

namespace nos::vk
{

struct nosVulkan_API Pipeline : DeviceChild
{
    rc<Shader> MainShader;
    rc<PipelineLayout> Layout;
    Pipeline(Device* Vk, std::vector<u8> const& src);
    Pipeline(Device* Vk, rc<Shader> CS);
	template <class T>
	void PushConstants(rc<CommandBuffer> Cmd, T const& data)
	{
		const auto size = std::min(size_t(Layout->PushConstantSize), sizeof(T));
        if(size)
        {
            Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, size, &data);
        }
	}
};

struct nosVulkan_API ComputePipeline : SharedFactory<ComputePipeline>, Pipeline
{
    ComputePipeline(Device* Vk, std::vector<u8> const& src);
    ComputePipeline(Device* Vk, rc<Shader> CS);
    VkPipeline Handle = 0;
};

struct nosVulkan_API GraphicsPipeline : SharedFactory<GraphicsPipeline>, Pipeline
{
    struct BlendMode
    {
        /*VkBlendOp*/     u16 Op: 8 = VK_BLEND_OP_ADD;
        /*VkBlendFactor*/ u16 SrcFactor : 4 = VK_BLEND_FACTOR_SRC_ALPHA;
        /*VkBlendFactor*/ u16 DstFactor : 4 = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    };
    
    rc<Shader> VS = nullptr;
    BlendMode Blend = {};
    VkSampleCountFlags MS = 1;

    struct PerFormat
    {
        VkPipeline pl;
        VkPipeline wpl;
        VkRenderPass rp;
    };
    
    std::map<VkFormat, PerFormat> Handles;

    GraphicsPipeline(Device* Vk, std::vector<u8> const&, BlendMode blend = {}, u32 MS = 1);
    GraphicsPipeline(Device* Vk, rc<Shader> PS, rc<Shader> VS = 0, BlendMode blend = {}, u32 MS = 1);
    ~GraphicsPipeline();

    rc<Shader> GetVS();

    void Recreate(VkFormat fmt);
};

} // namespace nos::vk