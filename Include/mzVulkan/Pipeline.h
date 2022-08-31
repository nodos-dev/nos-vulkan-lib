
#pragma once

#include <Layout.h>
#include <Shader.h>
#include <Command.h>

namespace mz::vk
{

struct mzVulkan_API Pipeline : SharedFactory<Pipeline>, DeviceChild
{
    inline static rc<Shader> GlobVS;
    rc<Shader> VS = nullptr;
    rc<Shader> PS;
    rc<PipelineLayout> Layout;
    
    struct PerFormat
    {
        VkPipeline pl;
        VkPipeline wpl;
        VkRenderPass rp;
    };
    
    std::map<VkFormat, PerFormat> Handles;

    Pipeline(Device* Vk, View<u8> src);
    Pipeline(Device* Vk, rc<Shader> PS, rc<Shader> VS = 0);
    ~Pipeline();

    rc<Shader> GetVS();

    void Recreate(VkFormat fmt);

    template <class T>
    void PushConstants(rc<CommandBuffer> Cmd, T const& data)
    {
        Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
    }

};

} // namespace mz::vk