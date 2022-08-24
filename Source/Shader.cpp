
#include <Shader.h>

namespace mz::vk
{

Shader::Shader(Device* Vk, View<u8> src)
    : DeviceChild(Vk)
{
    Layout = GetShaderLayouts(src, Stage, Binding, Attributes);
    
    VkShaderModuleCreateInfo info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = src.size(),
        .pCode    = (u32*)src.data(),
    };
    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateShaderModule(&info, 0, &Module));
}

Shader::~Shader()
{
    Vk->DestroyShaderModule(Module, 0);
}

bool Shader::GetInputLayout(VkPipelineVertexInputStateCreateInfo* info) const
{
    if(!((VK_SHADER_STAGE_VERTEX_BIT & Stage) && info))
    {
        return false;
    }

    *info = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = (u32)Attributes.size(),
        .pVertexAttributeDescriptions    = Attributes.data(),
    };


    if (!Attributes.empty())
    {
        info->vertexBindingDescriptionCount = 1;
        info->pVertexBindingDescriptions    = &Binding;
    }

    return true;
}

} // namespace mz::vk