/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */


#pragma once

#include "Device.h"

namespace nos::vk
{

struct nosVulkan_API Shader : SharedFactory<Shader>, DeviceChild
{
    VkShaderModule Module;
    VkShaderStageFlags Stage;
    ShaderLayout Layout;
    VkVertexInputBindingDescription Binding;
    std::vector<VkVertexInputAttributeDescription> Attributes;
    
    Shader(Device* Vk, std::vector<u8> const& src);
    Shader(Device* Vk, std::vector<u8> const& src, VkShaderModule Module);
    ~Shader();
    bool GetInputLayout(VkPipelineVertexInputStateCreateInfo* info) const;
    static rc<Shader> Create(Device* Vk, std::vector<u8> const& src);
};

} // namespace nos::vk