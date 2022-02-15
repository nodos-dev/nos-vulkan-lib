#pragma once

#include "mzVkCommon.h"

#include <spirv_cross.hpp>

enum RW
{
    RW_NONE      = 0,
    RW_READONLY  = 1,
    RW_WRITEONLY = 2,
    RW_UNUSABLE  = 3,
};

struct DescriptorBinding
{
    VkDescriptorType type;
    u32              binding;
    u32              rw;
    u32              count;

    VkDescriptorSetLayoutBinding get(VkShaderStageFlags stage) const
    {
        return VkDescriptorSetLayoutBinding{
            .binding         = binding,
            .descriptorType  = type,
            .descriptorCount = count,
            .stageFlags      = stage,
        };
    }
};

struct DescriptorLayout
{
    VkDescriptorSetLayout     handle;
    vector<DescriptorBinding> bindings;
    VkShaderStageFlags        stage;

    VkResult create(VkDevice dev)
    {
        vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(this->bindings.size());
        for (auto& binding : this->bindings)
        {
            bindings.push_back(binding.get(stage));
        }

        VkDescriptorSetLayoutCreateInfo info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = (u32)bindings.size(),
            .pBindings    = bindings.data(),
        };

        return vkCreateDescriptorSetLayout(dev, &info, 0, &handle);
    }
};

struct PipelineLayout
{
    VkPipelineLayout                     handle;
    unordered_map<u32, DescriptorLayout> descriptors;
    VkDescriptorPool                     pool;
    VkShaderStageFlags                   stage              = 0;
    u32                                  push_constant_size = 0;

    void create_pool(VkDevice dev)
    {
        unordered_map<VkDescriptorType, u32> counter;
        for (auto& [_, set] : descriptors)
        {
            for (auto& binding : set.bindings)
            {
                counter[binding.type] += binding.count;
            }
        }

        vector<VkDescriptorPoolSize> pools;
        pools.reserve(counter.size());
        for (auto& [type, count] : counter)
        {
            pools.push_back(VkDescriptorPoolSize{.type = type, .descriptorCount = count * 1024});
        }

        VkDescriptorPoolCreateInfo info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = (u32)descriptors.size() * 2048u,
            .poolSizeCount = (u32)pools.size(),
            .pPoolSizes    = pools.data(),
        };

        CHECKRE(vkCreateDescriptorPool(dev, &info, 0, &pool));
    }

    void add_stage(vector<u32> const& bin, string* name, VkShaderStageFlagBits* stage)
    {
        using namespace spirv_cross;
        Compiler        cc(bin.data(), bin.size());
        ShaderResources resources = cc.get_shader_resources();

        {
            EntryPoint entry = cc.get_entry_points_and_stages()[0];

            *name  = std::move(entry.name);
            *stage = VkShaderStageFlagBits(1 << (u32)entry.execution_model);
            this->stage |= *stage;
        }

        for (auto& pc : resources.push_constant_buffers)
        {
            push_constant_size = std::max(push_constant_size, (u32)cc.get_declared_struct_size(cc.get_type(pc.type_id)));
        }

        pair<VkDescriptorType, SmallVector<Resource>*> res[] = {
            pair{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &resources.sampled_images},
            pair{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &resources.separate_images},
            pair{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resources.storage_images},
            pair{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resources.storage_buffers},
            pair{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &resources.uniform_buffers},
            pair{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &resources.subpass_inputs},
        };

        for (auto& [ty, desc] : res)
        {
            for (auto& res : *desc)
            {
                SmallVector<u32> array = cc.get_type(res.type_id).array;
                u32              set   = cc.get_decoration(res.id, spv::DecorationBinding);
                descriptors[set].stage |= *stage;
                descriptors[set].bindings.push_back(
                    DescriptorBinding{
                        .type    = ty,
                        .binding = cc.get_decoration(res.id, spv::DecorationDescriptorSet),
                        .rw      = cc.get_decoration(res.id, spv::DecorationNonWritable) * RW_READONLY + cc.get_decoration(res.id, spv::DecorationNonReadable) * RW_WRITEONLY,
                        .count   = std::accumulate(array.begin(), array.end(), 1u, [](u32 a, u32 b) { return a * b; }),
                    });
            }
        };
    }

    void Create(VkDevice dev)
    {

        vector<VkDescriptorSetLayout> handles;
        handles.reserve(descriptors.size());
        for (auto& [_, set] : descriptors)
        {
            std::sort(set.bindings.begin(), set.bindings.end(), [](auto& l, auto& r) { return l.binding < r.binding; });
            set.create(dev);
            handles.push_back(set.handle);
        }

        VkPushConstantRange pc_range[1] = {
            {
                .stageFlags = stage,
                .size       = push_constant_size,
            },
        };

        VkPipelineLayoutCreateInfo layout_info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = (u32)handles.size(),
            .pSetLayouts            = handles.data(),
            .pushConstantRangeCount = push_constant_size > 0,
            .pPushConstantRanges    = pc_range,
        };

        CHECKRE(vkCreatePipelineLayout(dev, &layout_info, 0, &handle));

        create_pool(dev);
    }

    void dump()
    {
        const char* to_string(VkDescriptorType ty);

        for (auto& [set, layout] : descriptors)
        {
            printf("Set %u:\n", set);
            for (auto& [type, binding, rw, count] : layout.bindings)
            {
                printf("\t Binding %u:%s\n", binding, to_string(type));
            }
        }
    }

    VkResult AllocateSet(VkDevice dev, u32 idx, VkDescriptorSet* set)
    {
        return AllocateSets(dev, idx, 1, set);
    }

    VkResult AllocateSets(VkDevice dev, u32 idx, u32 num_sets, VkDescriptorSet* out_sets)
    {
        VkDescriptorSetAllocateInfo info = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = pool,
            .descriptorSetCount = num_sets,
            .pSetLayouts        = &descriptors[idx].handle,
        };
        return vkAllocateDescriptorSets(dev, &info, out_sets);
    }
};

struct Shaderx
{
    string                entry;
    VkShaderStageFlagBits stage;
    VkShaderModule        mod;

    VkResult Create(VkDevice dev, PipelineLayout* layout, vector<u32> const& bin)
    {
        layout->add_stage(bin, &entry, &stage);

        VkShaderModuleCreateInfo info = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = (u32)bin.size() * 4u,
            .pCode    = bin.data(),
        };

        return vkCreateShaderModule(dev, &info, 0, &mod);
    }
};

inline const char* to_string(VkDescriptorType ty)
{
    switch (ty)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        return "SAMPLER";
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return "COMBINED_IMAGE_SAMPLER";
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "SAMPLED_IMAGE";
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "STORAGE_IMAGE";
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return "UNIFORM_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return "STORAGE_TEXEL_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "UNIFORM_BUFFER";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "STORAGE_BUFFER";
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return "UNIFORM_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return "STORAGE_BUFFER_DYNAMIC";
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return "INPUT_ATTACHMENT";
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
        return "INLINE_UNIFORM_BLOCK_EXT";
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return "ACCELERATION_STRUCTURE_KHR";
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV:
        return "ACCELERATION_STRUCTURE_NV";
    case VK_DESCRIPTOR_TYPE_MUTABLE_VALVE:
        return "MUTABLE_VALVE";
    default:
        return "";
    }
}