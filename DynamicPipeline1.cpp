
#include "DynamicPipeline.h"

#include <spirv_cross.hpp>


#ifndef MZ_SHADER_PATH
#define MZ_SHADER_PATH "Shaders"
#endif


std::pair<VkFormat, u32> TypeAttributes(spirv_cross::SPIRType ty)
{

#define MKFORMATWT(w, t)                           \
    {                                              \
        VK_FORMAT_R##w##_##t,                      \
            VK_FORMAT_R##w##G##w##_##t,            \
            VK_FORMAT_R##w##G##w##B##w##_##t,      \
            VK_FORMAT_R##w##G##w##B##w##A##w##_##t \
    }

#define MKFORMAT(t)            \
    {                          \
        MKFORMATWT(8, t),      \
            MKFORMATWT(16, t), \
            MKFORMATWT(32, t), \
            MKFORMATWT(64, t)  \
    }

    constexpr VkFormat VK_FORMAT_R8_SFLOAT       = VK_FORMAT_UNDEFINED;
    constexpr VkFormat VK_FORMAT_R8G8_SFLOAT     = VK_FORMAT_UNDEFINED;
    constexpr VkFormat VK_FORMAT_R8G8B8_SFLOAT   = VK_FORMAT_UNDEFINED;
    constexpr VkFormat VK_FORMAT_R8G8B8A8_SFLOAT = VK_FORMAT_UNDEFINED;

    constexpr VkFormat SFLOAT[4][4] = MKFORMAT(SFLOAT);
    constexpr VkFormat SINT[4][4]   = MKFORMAT(SINT);
    constexpr VkFormat UINT[4][4]   = MKFORMAT(UINT);

    const u32 BitIdx = (ty.width >> 3) - 1;

    VkFormat fmt = VK_FORMAT_UNDEFINED;

    switch (ty.basetype)
    {
    case spirv_cross::SPIRType::Double:
    case spirv_cross::SPIRType::Float:
    case spirv_cross::SPIRType::Half:
        fmt = SFLOAT[BitIdx][ty.vecsize];
        break;
    case spirv_cross::SPIRType::Int64:
    case spirv_cross::SPIRType::Int:
    case spirv_cross::SPIRType::Short:
    case spirv_cross::SPIRType::SByte:
        fmt = SINT[BitIdx][ty.vecsize];
        break;
    case spirv_cross::SPIRType::UInt64:
    case spirv_cross::SPIRType::UInt:
    case spirv_cross::SPIRType::UShort:
    case spirv_cross::SPIRType::UByte:
        fmt = UINT[BitIdx][ty.vecsize];
        break;
    default:
        break;
    }

    return std::make_pair(fmt, ty.vecsize * ty.width / 8);
}

void ReadInputLayout(const u32* src, u64 sz, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes)
{
    using namespace spirv_cross;
    Compiler        cc(src, sz / 4);
    ShaderResources resources = cc.get_shader_resources();

    binding = {.inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

    attributes.clear();
    attributes.reserve(resources.stage_inputs.size());

    u32 location = 0;

    for (auto& input : resources.stage_inputs)
    {
        auto ty = cc.get_type(input.type_id);

        auto [format, size] = TypeAttributes(ty);

        attributes.emplace_back(VkVertexInputAttributeDescription{
            .location = location++,
            .binding  = 0,
            .format   = format,
            .offset   = binding.stride,
        });

        binding.stride += size;
    }
}
