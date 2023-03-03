/*
 * Copyright MediaZ AS. All Rights Reserved.
 */


#pragma once

#include "Layout.h"
#include "Shader.h"
#include "Command.h"

namespace mz::vk
{

struct mzVulkan_API Pipeline : DeviceChild
{
    rc<Shader> MainShader;
    rc<PipelineLayout> Layout;
    Pipeline(Device* Vk, std::vector<u8> const& src);
    Pipeline(Device* Vk, rc<Shader> CS);
    template <class T>
    void PushConstants(rc<CommandBuffer> Cmd, T const& data)
    {
        if (Layout->PushConstantSize >= sizeof(T))
        {
            Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
        }
    }
};

struct mzVulkan_API ComputePipeline : SharedFactory<ComputePipeline>, Pipeline
{
    ComputePipeline(Device* Vk, std::vector<u8> const& src);
    ComputePipeline(Device* Vk, rc<Shader> CS);
    VkPipeline Handle = 0;
};

struct mzVulkan_API GraphicsPipeline : SharedFactory<GraphicsPipeline>, Pipeline
{
    rc<Shader> VS = nullptr;
    bool EnableBlending = false;

    struct PerFormat
    {
        VkPipeline pl;
        VkPipeline wpl;
        VkRenderPass rp;
    };
    
    std::map<VkFormat, PerFormat> Handles;

    GraphicsPipeline(Device* Vk, std::vector<u8> const&, bool blend = false);
    GraphicsPipeline(Device* Vk, rc<Shader> PS, rc<Shader> VS = 0, bool blend = false);
    ~GraphicsPipeline();

    rc<Shader> GetVS();

    void Recreate(VkFormat fmt);

    template <class T>
    void PushConstants(rc<CommandBuffer> Cmd, T const& data)
    {
        if (Layout->PushConstantSize >= sizeof(T))
        {
            Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
        }
    }
};

} // namespace mz::vk