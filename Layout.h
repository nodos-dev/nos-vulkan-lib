#pragma once

#include "Image.h"
#include "vulkan/vulkan_core.h"
#include <type_traits>
#include <variant>

namespace mz
{

struct Binding
{
    using Type = std::variant<VulkanBuffer*, VulkanImage*>;

    Type resource;

    u32 binding;

    DescriptorResourceInfo info;

    VkAccessFlags access;

    auto operator<=>(const Binding& other) const
    {
        return binding <=> other.binding;
    }

    Binding(std::shared_ptr<VulkanBuffer> res, u32 binding)
        : Binding(res.get(), binding)
    {
    }

    Binding(VulkanBuffer* res, u32 binding)
        : resource(res), binding(binding), info(res->GetDescriptorInfo())
    {
    }

    Binding(std::shared_ptr<VulkanImage> res, u32 binding)
        : Binding(res.get(), binding)
    {
    }

    Binding(VulkanImage* res, u32 binding)
        : resource(res), binding(binding), info(res->GetDescriptorInfo())
    {
    }

    void Bind() const
    {
        if (VulkanImage* const* ppimage = std::get_if<VulkanImage*>(&resource))
        {
            (**ppimage).Transition(info.image.imageLayout, access);
        }
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
            assert(std::holds_alternative<VulkanImage*>(resource));
            Usage = std::get<VulkanImage*>(resource)->Usage;
            break;
        default:
            assert(std::holds_alternative<VulkanBuffer*>(resource));
            Usage = std::get<VulkanBuffer*>(resource)->Usage;
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
            .pImageInfo      = (std::holds_alternative<VulkanImage*>(resource) ? (&info.image) : 0),
            .pBufferInfo     = (std::holds_alternative<VulkanBuffer*>(resource) ? (&info.buffer) : 0),
        };
    }
};

struct DescriptorLayout : SharedFactory<DescriptorLayout>
{
    VulkanDevice* Vk;

    VkDescriptorSetLayout Handle;

    std::vector<VkDescriptorSetLayoutBinding> Bindings;

    DescriptorLayout(VulkanDevice* Vk, std::vector<VkDescriptorSetLayoutBinding> bindings)
        : Vk(Vk), Bindings(std::move(bindings))
    {
        VkDescriptorSetLayoutCreateInfo info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = (u32)Bindings.size(),
            .pBindings    = Bindings.data(),
        };

        MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateDescriptorSetLayout(&info, 0, &Handle));
    }

    ~DescriptorLayout()
    {
        Vk->DestroyDescriptorSetLayout(Handle, 0);
    }
};

struct DescriptorPool : SharedFactory<DescriptorPool>
{
    struct PipelineLayout* Layout;

    VkDescriptorPool Handle;

    std::vector<VkDescriptorPoolSize> Sizes;

    DescriptorPool(PipelineLayout* Layout);
    DescriptorPool(PipelineLayout* Layout, std::vector<VkDescriptorPoolSize> Sizes);

    ~DescriptorPool();
};

struct DescriptorSet : SharedFactory<DescriptorSet>
{
    DescriptorPool*   Pool;
    DescriptorLayout* Layout;
    u32               Index;

    VkDescriptorSet Handle;

    DescriptorSet(DescriptorPool*, u32);

    ~DescriptorSet();

    std::set<Binding> Bound;

    VkDescriptorType GetType(u32 Binding);

    template <class... Resource>
    requires(std::is_same_v<Resource, Binding>&&...)
        std::shared_ptr<DescriptorSet> UpdateWith(Resource... res)
    {
        VkWriteDescriptorSet writes[sizeof...(Resource)] = {res.GetDescriptorInfo(Handle, GetType(res.binding))...};
        Layout->Vk->UpdateDescriptorSets(sizeof...(Resource), writes, 0, 0);
        Bound.insert(res...);
        return shared_from_this();
    }

    void Bind(std::shared_ptr<CommandBuffer> Cmd);
};

struct PipelineLayout : SharedFactory<PipelineLayout>
{
    VulkanDevice* Vk;

    VkPipelineLayout Handle;

    std::shared_ptr<DescriptorPool> Pool;

    u32 PushConstantSize;
    u32 RTcount;

    std::map<u32, std::shared_ptr<DescriptorLayout>> Descriptors;

    ~PipelineLayout()
    {
        Vk->DestroyPipelineLayout(Handle, 0);
    }

    std::shared_ptr<DescriptorSet> AllocateSet(u32 set)
    {
        return DescriptorSet::New(Pool.get(), set);
    }

    void Dump();

    PipelineLayout(VulkanDevice* Vk, const u32* src, u64 sz);

  private:
    PipelineLayout(VulkanDevice* Vk, std::map<u32, std::vector<VkDescriptorSetLayoutBinding>> layouts);
};
} // namespace mz