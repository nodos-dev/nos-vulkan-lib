
#include "DynamicPipeline.h"

#include <spirv_cross.hpp>

std::pair<VkFormat, u32> TypeAttributes(spirv_cross::SPIRType ty)
{

#define MKFORMAT1(width, type) VK_FORMAT_R##width##_##type
#define MKFORMAT2(width, type) VK_FORMAT_R##width##G##width##_##type
#define MKFORMAT3(width, type) VK_FORMAT_R##width##G##width##B##width##_##type
#define MKFORMAT4(width, type) VK_FORMAT_R##width##G##width##B##width##A##width##_##type

#define MKFORMAT(width, type, vecsize) MKFORMAT##vecsize(width, type)

#define MKFORMAT_ARRAY(width, vecsize)       \
    {                                        \
        MKFORMAT(width, UINT, vecsize),      \
            MKFORMAT(width, SINT, vecsize),  \
            MKFORMAT(width, SFLOAT, vecsize) \
    }

#define MKFORMAT_ARRAY1(width)                                                                                 \
    {                                                                                                          \
        MKFORMAT_ARRAY(width, 1), MKFORMAT_ARRAY(width, 2), MKFORMAT_ARRAY(width, 3), MKFORMAT_ARRAY(width, 4) \
    }

    constexpr VkFormat VK_FORMAT_R8_SFLOAT       = VK_FORMAT_UNDEFINED;
    constexpr VkFormat VK_FORMAT_R8G8_SFLOAT     = VK_FORMAT_UNDEFINED;
    constexpr VkFormat VK_FORMAT_R8G8B8_SFLOAT   = VK_FORMAT_UNDEFINED;
    constexpr VkFormat VK_FORMAT_R8G8B8A8_SFLOAT = VK_FORMAT_UNDEFINED;

    constexpr VkFormat Map[4][4][3] = {MKFORMAT_ARRAY1(8),
                                       MKFORMAT_ARRAY1(16),
                                       MKFORMAT_ARRAY1(32),
                                       MKFORMAT_ARRAY1(64)};

    u32 BitIdx  = (ty.width >> 3) - 1;
    u32 VecIdx  = ty.vecsize;
    u32 TypeIdx = 0;

    switch (ty.basetype)
    {
    case spirv_cross::SPIRType::Double:
    case spirv_cross::SPIRType::Float:
    case spirv_cross::SPIRType::Half:
        TypeIdx = 2;
        break;
    case spirv_cross::SPIRType::Int64:
    case spirv_cross::SPIRType::Int:
    case spirv_cross::SPIRType::Short:
    case spirv_cross::SPIRType::SByte:
        TypeIdx = 1;
        break;
    case spirv_cross::SPIRType::UInt64:
    case spirv_cross::SPIRType::UInt:
    case spirv_cross::SPIRType::UShort:
    case spirv_cross::SPIRType::UByte:
        TypeIdx = 0;
        break;
    default:
        break;
    }

    return std::make_pair(Map[BitIdx][VecIdx][TypeIdx], ty.vecsize * ty.width / 8);
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
