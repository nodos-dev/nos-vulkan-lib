
#include <Shader.h>

namespace mz::vk
{

Shader::Shader(Device* Vk, VkShaderStageFlags stage, View<u8> src)
    : DeviceChild(Vk), Stage(stage)
{
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

PixelShader::PixelShader(Device* Vk, View<u8> src)
    : Shader(Vk, VK_SHADER_STAGE_FRAGMENT_BIT, src)
{

}

VertexShader::VertexShader(Device* Vk, View<u8> src)
    : Shader(Vk, VK_SHADER_STAGE_VERTEX_BIT, src)
{
    ReadInputLayout(src, Binding, Attributes);
}

VkPipelineVertexInputStateCreateInfo VertexShader::GetInputLayout() const
{
    VkPipelineVertexInputStateCreateInfo InputLayout = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = (u32)Attributes.size(),
        .pVertexAttributeDescriptions    = Attributes.data(),
    };

    if (!Attributes.empty())
    {
        InputLayout.vertexBindingDescriptionCount = 1;
        InputLayout.pVertexBindingDescriptions    = &Binding;
    }

    return InputLayout;
}

} // namespace mz::vk