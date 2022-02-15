
#include "DynamicPipeline.h"
#include "spirv_common.hpp"
#include "vulkan/vulkan_core.h"

#include <memory>
#include <shaderc/shaderc.hpp>

#include <spirv_cross.hpp>

shaderc_shader_kind MapShaderKind(MZShaderKind s)
{
    switch (s)
    {
    case MZ_SHADER_COMPUTE:
        return shaderc_compute_shader;
    case MZ_SHADER_PIXEL:
        return shaderc_fragment_shader;
    default:
        return shaderc_vertex_shader;
    }
}

shaderc_optimization_level MapOptLevel(MZOptLevel o)
{
    switch (o)
    {
    case MZ_OPT_LEVEL_SIZE:
        return shaderc_optimization_level_size;
    case MZ_OPT_LEVEL_PERF:
        return shaderc_optimization_level_performance;
    default:
        return shaderc_optimization_level_zero;
    }
}

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

void ReadInputLayout(std::vector<u32> const& bin, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes)
{
    using namespace spirv_cross;
    Compiler        cc(bin.data(), bin.size());
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

std::vector<uint32_t> CompileFile(const std::string&                              source_name,
                                  MZShaderKind                                    kind,
                                  const std::string&                              source,
                                  std::string&                                    err,
                                  enum MZOptLevel                                 opt,
                                  VkVertexInputBindingDescription*                binding,
                                  std::vector<VkVertexInputAttributeDescription>* attributes)
{
    shaderc::Compiler       compiler;
    shaderc::CompileOptions options;

    // // Like -DMY_DEFINE=1
    // options.AddMacroDefinition("MY_DEFINE", "1");
    options.SetOptimizationLevel(MapOptLevel(opt));

    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(source, MapShaderKind(kind), source_name.c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        err = module.GetErrorMessage();
        return std::vector<u32>();
    }

    std::vector<u32> bin(module.cbegin(), module.cend());

    if (kind == MZ_SHADER_VERTEX && binding && attributes)
    {
        ReadInputLayout(bin, *binding, *attributes);
    }

    return bin;
}