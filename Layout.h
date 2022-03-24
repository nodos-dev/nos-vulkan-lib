#pragma once

#include "Image.h"
#include "mzVkCommon.h"
#include "vulkan/vulkan_core.h"
#include <type_traits>
#include <variant>

namespace mz::vk
{

template <class T>
concept TypeClassResource = TypeClassImage<T> || TypeClassBuffer<T>;

struct MZVULKAN_API Binding
{
    using Type = std::variant<Buffer*, Image*>;

    Type resource;

    u32 binding;

    DescriptorResourceInfo info;

    VkAccessFlags access;

    auto operator<=>(const Binding& other) const
    {
        return binding <=> other.binding;
    }

    Binding(std::shared_ptr<Buffer> res, u32 binding)
        : Binding(res.get(), binding)
    {
    }

    Binding(Buffer* res, u32 binding)
        : resource(res), binding(binding), info(res->GetDescriptorInfo())
    {
    }

    Binding(std::shared_ptr<Image> res, u32 binding)
        : Binding(res.get(), binding)
    {
    }

    Binding(Image* res, u32 binding)
        : resource(res), binding(binding), info(res->GetDescriptorInfo())
    {
    }

    void SanityCheck(VkDescriptorType type)
    {
        VkFlags Usage = 0;

        switch (type)
        {
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            assert(std::holds_alternative<Image*>(resource));
            Usage = std::get<Image*>(resource)->Usage;
            break;
        default:
            assert(std::holds_alternative<Buffer*>(resource));
            Usage = std::get<Buffer*>(resource)->Usage;
            break;
        }

        switch (type)
        {
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            assert(Usage & VK_IMAGE_USAGE_SAMPLED_BIT);
            info.image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            access                 = VK_ACCESS_SHADER_READ_BIT;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            assert(Usage & VK_IMAGE_USAGE_STORAGE_BIT);
            info.image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            access                 = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            assert(Usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
            info.image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            access                 = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            assert(Usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            assert(Usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
            assert(Usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            assert(Usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            break;
        default:
            assert(0);
            break;
        }
    }

    VkWriteDescriptorSet GetDescriptorInfo(VkDescriptorSet set, VkDescriptorType type)
    {
        SanityCheck(type);

        return VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pImageInfo      = (std::holds_alternative<Image*>(resource) ? (&info.image) : 0),
            .pBufferInfo     = (std::holds_alternative<Buffer*>(resource) ? (&info.buffer) : 0),
        };
    }
};

struct MZVULKAN_API DescriptorLayout : SharedFactory<DescriptorLayout>
{
    Device* Vk;

    VkDescriptorSetLayout Handle;

    std::map<u32, NamedDSLBinding> Bindings;

    NamedDSLBinding const& operator[](u32 binding) const
    {
        return Bindings.at(binding);
    }

    auto begin() const
    {
        return Bindings.begin();
    }

    auto end() const
    {
        return Bindings.end();
    }

    DescriptorLayout(Device* Vk, std::map<u32, NamedDSLBinding> NamedBindings)
        : Vk(Vk), Bindings(std::move(NamedBindings))
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(Bindings.size());

        for (auto& [i, b] : Bindings)
        {
            bindings.emplace_back(VkDescriptorSetLayoutBinding{
                .binding         = i,
                .descriptorType  = b.descriptorType,
                .descriptorCount = b.descriptorCount,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
        }

        VkDescriptorSetLayoutCreateInfo info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = (u32)bindings.size(),
            .pBindings    = bindings.data(),
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateDescriptorSetLayout(&info, 0, &Handle));
    }

    ~DescriptorLayout()
    {
        Vk->DestroyDescriptorSetLayout(Handle, 0);
    }
};

struct MZVULKAN_API DescriptorPool : SharedFactory<DescriptorPool>
{
    struct PipelineLayout* Layout;

    VkDescriptorPool Handle;

    std::vector<VkDescriptorPoolSize> Sizes;

    DescriptorPool(PipelineLayout* Layout);
    DescriptorPool(PipelineLayout* Layout, std::vector<VkDescriptorPoolSize> Sizes);

    ~DescriptorPool();
};

struct MZVULKAN_API DescriptorSet : SharedFactory<DescriptorSet>
{
    DescriptorPool*   Pool;
    DescriptorLayout* Layout;
    u32               Index;

    VkDescriptorSet Handle;

    DescriptorSet(DescriptorPool*, u32);

    ~DescriptorSet();

    std::set<Binding> Bound;

    VkDescriptorType GetType(u32 Binding);

    template <std::same_as<Binding>... Bindings>
    std::shared_ptr<DescriptorSet> UpdateWith(Bindings&&... res)
    {
        VkWriteDescriptorSet writes[sizeof...(Bindings)] = {res.GetDescriptorInfo(Handle, GetType(res.binding))...};
        Layout->Vk->UpdateDescriptorSets(sizeof...(Bindings), writes, 0, 0);
        Bound.insert(res...);
        return shared_from_this();
    }

    std::shared_ptr<DescriptorSet> Bind(std::shared_ptr<CommandBuffer> Cmd);
};

struct MZVULKAN_API PipelineLayout : SharedFactory<PipelineLayout>
{
    Device* Vk;

    VkPipelineLayout Handle;

    std::shared_ptr<DescriptorPool> Pool;

    u32 PushConstantSize;
    u32 RTCount;

    std::map<u32, std::shared_ptr<DescriptorLayout>> DescriptorSets;
    std::unordered_map<std::string, glm::uvec2>      BindingsByName;

    DescriptorLayout const& operator[](u32 set) const
    {
        return *DescriptorSets.at(set);
    }

    auto begin() const
    {
        return DescriptorSets.begin();
    }

    auto end() const
    {
        return DescriptorSets.end();
    }

    ~PipelineLayout()
    {
        Vk->DestroyPipelineLayout(Handle, 0);
    }

    std::shared_ptr<DescriptorSet> AllocateSet(u32 set)
    {
        return DescriptorSet::New(Pool.get(), set);
    }

    void Dump();

    PipelineLayout(Device* Vk, View<u8> src);

  private:
    PipelineLayout(Device* Vk, ShaderLayout layout);
};
} // namespace mz::vk