// Copyright MediaZ AS. All Rights Reserved.


// External
#include <spirv_cross.hpp>

// nosVulkan
#include "nosVulkan/Pipeline.h"

// std
#include <numeric>

namespace nos::vk
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
    VkFormat fmt = VK_FORMAT_UNDEFINED;

    switch (ty.vecsize)
    {
    case 2:
        switch (ty.basetype)
        {
        case spirv_cross::SPIRType::Double: fmt = VK_FORMAT_R64G64_SFLOAT; break;
        case spirv_cross::SPIRType::Float:  fmt = VK_FORMAT_R32G32_SFLOAT; break;
        case spirv_cross::SPIRType::Half:   fmt = VK_FORMAT_R16G16_SFLOAT; break;
        case spirv_cross::SPIRType::Int64:  fmt = VK_FORMAT_R64G64_SINT; break;
        case spirv_cross::SPIRType::Int:    fmt = VK_FORMAT_R32G32_SINT; break;
        case spirv_cross::SPIRType::Short:  fmt = VK_FORMAT_R16G16_SINT; break;
        case spirv_cross::SPIRType::SByte:  fmt = VK_FORMAT_R8G8_SINT; break;
        case spirv_cross::SPIRType::UInt64: fmt = VK_FORMAT_R64G64_UINT; break;
        case spirv_cross::SPIRType::UInt:   fmt = VK_FORMAT_R32G32_UINT; break;
        case spirv_cross::SPIRType::UShort: fmt = VK_FORMAT_R16G16_UINT; break;
        case spirv_cross::SPIRType::UByte:  fmt = VK_FORMAT_R8G8_UINT; break;
        default:
            break;
        }
        break;
    case 3:
        switch (ty.basetype)
        {
        case spirv_cross::SPIRType::Double: fmt = VK_FORMAT_R64G64B64_SFLOAT; break;
        case spirv_cross::SPIRType::Float:  fmt = VK_FORMAT_R32G32B32_SFLOAT; break;
        case spirv_cross::SPIRType::Half:   fmt = VK_FORMAT_R16G16B16_SFLOAT; break;
        case spirv_cross::SPIRType::Int64:  fmt = VK_FORMAT_R64G64B64_SINT; break;
        case spirv_cross::SPIRType::Int:    fmt = VK_FORMAT_R32G32B32_SINT; break;
        case spirv_cross::SPIRType::Short:  fmt = VK_FORMAT_R16G16B16_SINT; break;
        case spirv_cross::SPIRType::SByte:  fmt = VK_FORMAT_R8G8B8_SINT; break;
        case spirv_cross::SPIRType::UInt64: fmt = VK_FORMAT_R64G64B64_UINT; break;
        case spirv_cross::SPIRType::UInt:   fmt = VK_FORMAT_R32G32B32_UINT; break;
        case spirv_cross::SPIRType::UShort: fmt = VK_FORMAT_R16G16B16_UINT; break;
        case spirv_cross::SPIRType::UByte:  fmt = VK_FORMAT_R8G8B8_UINT; break;
        default:
            break;
        }
        break;
    case 4:
        switch (ty.basetype)
        {
        case spirv_cross::SPIRType::Double: fmt = VK_FORMAT_R64G64B64A64_SFLOAT; break;
        case spirv_cross::SPIRType::Float:  fmt = VK_FORMAT_R32G32B32A32_SFLOAT; break;
        case spirv_cross::SPIRType::Half:   fmt = VK_FORMAT_R16G16B16A16_SFLOAT; break;
        case spirv_cross::SPIRType::Int64:  fmt = VK_FORMAT_R64G64B64A64_SINT; break;
        case spirv_cross::SPIRType::Int:    fmt = VK_FORMAT_R32G32B32A32_SINT; break;
        case spirv_cross::SPIRType::Short:  fmt = VK_FORMAT_R16G16B16A16_SINT; break;
        case spirv_cross::SPIRType::SByte:  fmt = VK_FORMAT_R8G8B8A8_SINT; break;
        case spirv_cross::SPIRType::UInt64: fmt = VK_FORMAT_R64G64B64A64_UINT; break;
        case spirv_cross::SPIRType::UInt:   fmt = VK_FORMAT_R32G32B32A32_UINT; break;
        case spirv_cross::SPIRType::UShort: fmt = VK_FORMAT_R16G16B16A16_UINT; break;
        case spirv_cross::SPIRType::UByte:  fmt = VK_FORMAT_R8G8B8A8_UINT; break;
        default:
            break;
        }
    default:
        break;
    }


    return std::make_pair(fmt, ty.vecsize * ty.width / 8);
}

void ReadInputLayout(spirv_cross::Compiler& cc, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes)
{
    using namespace spirv_cross;
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

static void BuildType(spirv_cross::Compiler const& cc, u32 typeId, SVType* ty);

static rc<SVType> GetType(spirv_cross::Compiler const& cc, u32 typeId)
{
    static std::unordered_set<rc<SVType>> TypeCache;
    rc<SVType> tmp = std::make_shared<SVType>();
    BuildType(cc, typeId, tmp.get());
    if(auto it = TypeCache.find(tmp); it != TypeCache.end())
        return *it;
    TypeCache.insert(tmp);
    return tmp;
}

static void BuildType(spirv_cross::Compiler const& cc, u32 typeId, SVType* ty)
{
    using namespace spirv_cross;

    spirv_cross::SPIRType const& type = cc.get_type(typeId);
    ty->x = type.width;
    ty->y = type.vecsize;
    ty->z = type.columns;

    u32 v = (ty->y == 3 ? 4 : ty->y);

    ty->Alignment = v * ty->x / 8;
    ty->Size      = ty->Alignment * ty->z;
    ty->Alignment = std::max(1u, ty->Alignment);
    ty->ArraySize = 0;

    if(!type.array_size_literal.empty())
    {
        assert(1 == type.array_size_literal.size());
        assert(type.array_size_literal.front());;
        ty->ArraySize = type.array.front();
        ty->ArraySize = ty->ArraySize ? ty->ArraySize : ~0u;
    }
    
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
            auto name = cc.get_member_name(type.self, idx);
            ty->Members[name] = {
                .Type   = GetType(cc, type.member_types[idx]),
                .Idx    = idx,
                .Size   = (u32)cc.get_declared_struct_member_size(type, idx),
                .Offset = cc.type_struct_member_offset(type, idx),
            };
        }

        break;
    case SPIRType::Sampler:
    {
        assert("We dont support seperate samplers yet" && 0);
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

    if (ty->Size  && 0 != ty->ArraySize && ~0u != ty->ArraySize)
        ty->Size *= ty->ArraySize;
}

ShaderLayout GetShaderLayouts(std::vector<u8> const& src, VkShaderStageFlags& stage, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes)
{
    ShaderLayout layout = {};

    using namespace spirv_cross;

    Compiler cc((u32*)src.data(), src.size() / 4);
    ShaderResources resources = cc.get_shader_resources();
    EntryPoint entry          = cc.get_entry_points_and_stages()[0];

    stage = VkShaderStageFlagBits(1 << (u32)entry.execution_model);
    
    if(VK_SHADER_STAGE_VERTEX_BIT & stage)
    {
        ReadInputLayout(cc, binding, attributes);
    }
    if(VK_SHADER_STAGE_FRAGMENT_BIT & stage)
    {
        layout.RTCount = resources.stage_outputs.size();
    }

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
        // std::pair{VK_DESCRIPTOR_TYPE_SAMPLER, &resources.separate_samplers},
    };

    for (auto& [ty, desc] : res)
    {
        for (auto& res : *desc)
        {

            SPIRType const& type = cc.get_type(res.type_id);
            u32 set = cc.get_decoration(res.id, spv::DecorationDescriptorSet);
            u32 binding = cc.get_decoration(res.id, spv::DecorationBinding);
            u32 count = std::accumulate(type.array.begin(), type.array.end(), 1u, [](u32 a, u32 b) { return a * b; });
            NamedDSLBinding dsl = {
                .Binding         = binding,
                .DescriptorType  = ty,
                .DescriptorCount = count ? count : 16,
                .Name            = cc.get_name(res.id),
                .Type            = GetType(cc, res.type_id),
                .StageMask       = stage
            };
            
            ShaderLayout::Index idx = {set, binding, 0};
            layout.BindingsByName[cc.get_name(res.id)] = idx;
            if(SVType::Struct == dsl.Type->Tag)
            {
				auto flags = cc.get_buffer_block_flags(res.id);
				auto nonReadable = flags.get(spv::DecorationNonReadable);
				auto nonWriteable = flags.get(spv::DecorationNonWritable);
				auto access = AccessFlags((nonReadable ? 0 : AccessFlagRead) | (nonWriteable ? 0 : AccessFlagWrite));
				dsl.Access = access; // TODO: Non-buffer accesses, improve Image::TransitionInput
                for(auto& [name, member] : dsl.Type->Members)
                {
                    ShaderLayout::Index idx = {set, binding, member.Offset};
                    layout.BindingsByName[name] = idx;
                }
			}
			layout.DescriptorSets[set][binding] = dsl;
        }
    }

    return layout;
}

ShaderLayout ShaderLayout::Merge(ShaderLayout const& rhs) const
{
    ShaderLayout re = *this;
    re.BindingsByName.insert(rhs.BindingsByName.begin(), rhs.BindingsByName.end());
    re.RTCount = std::max(RTCount, rhs.RTCount);
    re.PushConstantSize = std::max(PushConstantSize, rhs.PushConstantSize);
    for(auto& [s, b] : rhs.BindingsByName)
    {
        re.BindingsByName[s] = b;
    }

    for(auto& [set, b] : rhs.DescriptorSets)
    {
        auto& desc = re.DescriptorSets[set];
        for(auto& [binding, dsl] : b)
        {
            auto& lhs = desc[binding];
            VkShaderStageFlags mask = lhs.StageMask;
            lhs = dsl;
            lhs.StageMask |= mask;
        }
    }
    return re;
}

} // namespace nos::vk