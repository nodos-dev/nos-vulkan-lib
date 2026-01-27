/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
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
		if (size)
		{
			Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, size, &data);
		}
	}
};

struct nosVulkan_API ComputePipeline : SharedFactory<ComputePipeline>, Pipeline
{
	ComputePipeline(Device* Vk, std::vector<u8> const& src);
	ComputePipeline(Device* Vk, rc<Shader> CS);
	~ComputePipeline();
	VkPipeline Handle = 0;
};
struct BlendMode
{
	/*VkBool*/ u32 Enable : 1 = false;
	/*VkBlendFactor*/ u32 SrcColorFactor : 5 = 0;
	/*VkBlendFactor*/ u32 DstColorFactor : 5 = 0;
	/*VkBlendFactor*/ u32 SrcAlphaFactor : 5 = 0;
	/*VkBlendFactor*/ u32 DstAlphaFactor : 5 = 0;
	/*VkColorComponentFlags*/ u32 ColorMask : 4 = 0xF;
	/*VkBlendOp*/ u32 ColorOp = 0;
	/*VkBlendOp*/ u32 AlphaOp = 0;
};

struct GraphicsPipelineKey
{
	std::vector<VkFormat> OutputFormats;
	std::optional<VkFormat> DepthFormat;

	auto operator<=>(const GraphicsPipelineKey& other) const = default;
};
} // namespace nos::vk

namespace std
{
template <>
struct hash<nos::vk::GraphicsPipelineKey>
{
	std::size_t operator()(const nos::vk::GraphicsPipelineKey& k) const
	{
		size_t hash = 0;
		nos::hash_combine(hash, k.DepthFormat);
		for (const auto& format : k.OutputFormats)
		{
			nos::hash_combine(hash, (u32)format);
		}
		return hash;
	}
};
} // namespace std

namespace nos::vk
{
struct nosVulkan_API GraphicsPipeline : SharedFactory<GraphicsPipeline>, Pipeline
{
    rc<Shader> VS = nullptr;
    BlendMode Blend = {};
    VkSampleCountFlags MS = 1;

    struct SpecializedPipeline
    {
        VkPipeline pl;
        VkPipeline wpl;
        VkRenderPass rp;
    };
    

    GraphicsPipeline(Device* Vk, std::vector<u8> const&, BlendMode blend = BlendMode(), u32 MS = 1);
    GraphicsPipeline(Device* Vk, rc<Shader> PS, rc<Shader> VS = 0, BlendMode blend = BlendMode(), u32 MS = 1);
    ~GraphicsPipeline();

    rc<Shader> GetVS();


	SpecializedPipeline const& CreateOrGetSpecializedPipeline(GraphicsPipelineKey const& key);

private:
	std::unordered_map<GraphicsPipelineKey, SpecializedPipeline> SpecializedPipelines;
};

} // namespace nos::vk
