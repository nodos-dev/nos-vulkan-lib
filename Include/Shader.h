
#pragma once

#include <Device.h>

namespace mz::vk
{

struct mzVulkan_API Shader : SharedFactory<Shader>, DeviceChild
{
    VkShaderModule Module;
    VkShaderStageFlags Stage;
    Shader(Device* Vk, VkShaderStageFlags stage, View<u8> src);
    ~Shader();
};

struct mzVulkan_API PixelShader : Shader
{
    PixelShader(Device* Vk, View<u8> src);
};

struct mzVulkan_API VertexShader : Shader
{
    VkVertexInputBindingDescription Binding;
    std::vector<VkVertexInputAttributeDescription> Attributes;
    VertexShader(Device* Vk, View<u8> src);
    VkPipelineVertexInputStateCreateInfo GetInputLayout() const;
};

} // namespace mz::vk