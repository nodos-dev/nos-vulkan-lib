
#pragma once

#include <Layout.h>
#include <Shader.h>
#include <Command.h>

namespace mz::vk
{

struct mzVulkan_API Pipeline : SharedFactory<Pipeline>, DeviceChild
{
    inline static VertexShader* VS = nullptr;
    rc<Shader> PS;
    rc<PipelineLayout> Layout;
    
    struct PerFormat
    {
        VkPipeline pl;
        VkRenderPass rp;
    };
    
    std::map<VkFormat, PerFormat> Handles;

    Pipeline(Device* Vk, View<u8> src);
    ~Pipeline();

    VertexShader* GetVS() const;

    void Recreate(VkFormat fmt);

    template <class T>
    void PushConstants(rc<CommandBuffer> Cmd, T const& data)
    {
        Cmd->PushConstants(Layout->Handle, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(T), &data);
    }

};

} // namespace mz::vk