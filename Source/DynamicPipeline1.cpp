#include <Pipeline.h>

#include <spirv_cross.hpp>

namespace mz::vk
{

static VkFormat MapSpvFormat(spv::ImageFormat format)
{
    using namespace spv;

    switch (format)
    {
    case ImageFormatRgba32f:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ImageFormatRgba16f:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormatR32f:
        return VK_FORMAT_R32_SFLOAT;
    case ImageFormatRgba8:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormatRgba8Snorm:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case ImageFormatRg32f:
        return VK_FORMAT_R32G32_SFLOAT;
    case ImageFormatRg16f:
        return VK_FORMAT_R16G16_SFLOAT;
    case ImageFormatR11fG11fB10f:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case ImageFormatR16f:
        return VK_FORMAT_R16_SFLOAT;
    case ImageFormatRgba16:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case ImageFormatRgb10A2:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case ImageFormatRg16:
        return VK_FORMAT_R16G16_UNORM;
    case ImageFormatRg8:
        return VK_FORMAT_R8G8_UNORM;
    case ImageFormatR16:
        return VK_FORMAT_R16_UNORM;
    case ImageFormatR8:
        return VK_FORMAT_R8_UNORM;
    case ImageFormatRgba16Snorm:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case ImageFormatRg16Snorm:
        return VK_FORMAT_R16G16_SNORM;
    case ImageFormatRg8Snorm:
        return VK_FORMAT_R8G8_SNORM;
    case ImageFormatR16Snorm:
        return VK_FORMAT_R16_SNORM;
    case ImageFormatR8Snorm:
        return VK_FORMAT_R8_SNORM;
    case ImageFormatRgba32i:
        return VK_FORMAT_R32G32B32A32_SINT;
    case ImageFormatRgba16i:
        return VK_FORMAT_R16G16B16A16_SINT;
    case ImageFormatRgba8i:
        return VK_FORMAT_R8G8B8A8_SINT;
    case ImageFormatR32i:
        return VK_FORMAT_R32_SINT;
    case ImageFormatRg32i:
        return VK_FORMAT_R32G32_SINT;
    case ImageFormatRg16i:
        return VK_FORMAT_R16G16_SINT;
    case ImageFormatRg8i:
        return VK_FORMAT_R8G8_SINT;
    case ImageFormatR16i:
        return VK_FORMAT_R16_SINT;
    case ImageFormatR8i:
        return VK_FORMAT_R8_SINT;
    case ImageFormatRgba32ui:
        return VK_FORMAT_R32G32B32A32_UINT;
    case ImageFormatRgba16ui:
        return VK_FORMAT_R16G16B16A16_UINT;
    case ImageFormatRgba8ui:
        return VK_FORMAT_R8G8B8A8_UINT;
    case ImageFormatR32ui:
        return VK_FORMAT_R32_UINT;
    case ImageFormatRgb10a2ui:
        return VK_FORMAT_A2R10G10B10_UINT_PACK32;
    case ImageFormatRg32ui:
        return VK_FORMAT_R32G32_UINT;
    case ImageFormatRg16ui:
        return VK_FORMAT_R16G16_UINT;
    case ImageFormatRg8ui:
        return VK_FORMAT_R8G8_UINT;
    case ImageFormatR16ui:
        return VK_FORMAT_R16_UINT;
    case ImageFormatR8ui:
        return VK_FORMAT_R8_UINT;
    case ImageFormatR64ui:
        return VK_FORMAT_R64_UINT;
    case ImageFormatR64i:
        return VK_FORMAT_R64_SINT;
    case ImageFormatUnknown:
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

static std::pair<VkFormat, u32> TypeAttributes(spirv_cross::SPIRType const& ty)
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

void ReadInputLayout(View<u8> bin, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes)
{
    using namespace spirv_cross;
    Compiler cc((u32*)bin.data(), bin.size() / 4);
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

static rc<SVType> GetType(spirv_cross::Compiler const& cc, u32 typeId, std::map<u32, rc<SVType>>& cache)
{
    using namespace spirv_cross;

    if (auto it = cache.find(typeId); it != cache.end())
    {
        return it->second;
    }

    SPIRType const& type = cc.get_type(typeId);

    rc<SVType> ty = std::make_shared<SVType>();

    cache[typeId] = ty;

    ty->x = type.width;
    ty->y = type.vecsize;
    ty->z = type.columns;

    u32 v = (ty->y == 3 ? 4 : ty->y);

    ty->Alignment = v * ty->x / 8;
    ty->Size      = ty->Alignment * ty->z;
    ty->Alignment = std::max(1u, ty->Alignment);

    switch (type.basetype)
    {

    case SPIRType::Struct:
        ty->Tag = SVType::Struct;

        // ty->members.resize(type.member_types.size());
        ty->Size = cc.get_declared_struct_size(type);

        for (u32 i = 0; i < type.member_types.size(); ++i)
        {
            u32 idx = i;

            if (idx < type.member_type_index_redirection.size())
            {
                idx = type.member_type_index_redirection[idx];
            }

            ty->Members[cc.get_member_name(typeId, idx)] = {
                .Type   = GetType(cc, type.member_types[idx], cache),
                .Idx    = idx,
                .Size   = (u32)cc.get_declared_struct_member_size(type, idx),
                .Offset = cc.type_struct_member_offset(type, idx),
            };
        }

        break;
    case SPIRType::Image:
    case SPIRType::SampledImage:
        ty->Tag  = SVType::Image;
        ty->Size = 0;

        ty->Img = {
            .Depth   = type.image.depth,
            .Array   = type.image.arrayed,
            .MS      = type.image.ms,
            .Read    = (spv::AccessQualifierReadOnly == type.image.access) || (spv::AccessQualifierReadWrite == type.image.access),
            .Write   = (spv::AccessQualifierWriteOnly == type.image.access) || (spv::AccessQualifierReadWrite == type.image.access),
            .Sampled = type.image.sampled,
            .Fmt     = MapSpvFormat(type.image.format),
        };

        break;
    case SPIRType::UInt64:
    case SPIRType::UInt:
    case SPIRType::UShort:
    case SPIRType::UByte:
        ty->Tag = SVType::Uint;
        break;
    case SPIRType::Int64:
    case SPIRType::Int:
    case SPIRType::Short:
    case SPIRType::SByte:
        ty->Tag = SVType::Sint;
        break;
    case SPIRType::Double:
    case SPIRType::Float:
    case SPIRType::Half:
        ty->Tag = SVType::Float;
        break;
    default:
        break;
    }

    return ty;
} // namespace mz::vk

ShaderLayout GetShaderLayouts(View<u8> src)
{
    ShaderLayout layout = {};

    using namespace spirv_cross;

    Compiler cc((u32*)src.data(), src.size() / 4);
    ShaderResources resources = cc.get_shader_resources();
    EntryPoint entry          = cc.get_entry_points_and_stages()[0];

    VkShaderStageFlags stage = VkShaderStageFlagBits(1 << (u32)entry.execution_model);

    layout.RTCount = resources.stage_outputs.size();

    if (!resources.push_constant_buffers.empty())
    {
        layout.PushConstantSize =
            cc.get_declared_struct_size(cc.get_type(resources.push_constant_buffers[0].type_id));
    }

    std::pair<VkDescriptorType, SmallVector<Resource>*> res[] = {
        std::pair{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &resources.sampled_images},
        std::pair{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &resources.separate_images},
        std::pair{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resources.storage_images},
        std::pair{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resources.storage_buffers},
        std::pair{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &resources.uniform_buffers},
        std::pair{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &resources.subpass_inputs},
    };

    std::map<u32, rc<SVType>> typeCache;

    for (auto& [ty, desc] : res)
    {
        for (auto& res : *desc)
        {

            SPIRType const& type = cc.get_type(res.type_id);

            u32 set     = cc.get_decoration(res.id, spv::DecorationDescriptorSet);
            u32 binding = cc.get_decoration(res.id, spv::DecorationBinding);
            NamedDSLBinding dsl = {
                .Binding         = binding,
                .DescriptorType  = ty,
                .DescriptorCount = std::accumulate(type.array.begin(), type.array.end(), 1u, [](u32 a, u32 b) { return a * b; }),
                .Name            = cc.get_name(res.id),
                .Type            = GetType(cc, res.base_type_id, typeCache),
                .StageMask       = stage,
            };
            
            ShaderLayout::Index idx = {set, binding, 0};
            layout.BindingsByName[cc.get_name(res.id)] = idx;
            layout.DescriptorSets[set][binding] = dsl;
            if(SVType::Struct == dsl.Type->Tag)
            {
                for(auto& [name, member] : dsl.Type->Members)
                {
                    ShaderLayout::Index idx = {set, binding, member.Offset};
                    layout.BindingsByName[name] = idx;
                }
            }
        }
    };

    return layout;
}
} // namespace mz::vk