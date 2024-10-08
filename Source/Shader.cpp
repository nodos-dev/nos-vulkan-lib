// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


#include "nosVulkan/Shader.h"

namespace nos::vk
{

rc<Shader> Shader::Create(Device* Vk, std::vector<u8> const& src)
{
    VkShaderModule handle;
    VkShaderModuleCreateInfo info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = src.size(),
        .pCode    = (u32*)src.data(),
    };
    if(VK_SUCCESS == Vk->CreateShaderModule(&info, 0, &handle))
        return Shader::New(Vk, src, handle);
    return nullptr;
}

Shader::Shader(Device* Vk, std::vector<u8> const& src, VkShaderModule Module) 
    : DeviceChild(Vk), Module(Module)
{
    Layout = GetShaderLayouts(src, Stage, Binding, Attributes);
}

Shader::Shader(Device* Vk, std::vector<u8> const& src)
    : DeviceChild(Vk)
{
    Layout = GetShaderLayouts(src, Stage, Binding, Attributes);
    
    VkShaderModuleCreateInfo info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = src.size(),
        .pCode    = (u32*)src.data(),
    };
    NOSVK_ASSERT(Vk->CreateShaderModule(&info, 0, &Module));
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

} // namespace nos::vk