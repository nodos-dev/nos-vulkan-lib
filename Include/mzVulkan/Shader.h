/*
 * // Copyright MediaZ AS. All Rights Reserved.
 */


#pragma once

#include "Device.h"

namespace mz::vk
{

struct mzVulkan_API Shader : SharedFactory<Shader>, DeviceChild
{
    VkShaderModule Module;
    VkShaderStageFlags Stage;
    ShaderLayout Layout;
    Shader(Device* Vk, View<u8> src);
    ~Shader();
    VkVertexInputBindingDescription Binding;
    std::vector<VkVertexInputAttributeDescription> Attributes;
    bool GetInputLayout(VkPipelineVertexInputStateCreateInfo* info) const;
};

} // namespace mz::vk