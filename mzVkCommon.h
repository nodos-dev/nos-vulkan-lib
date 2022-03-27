#pragma once

#include <vkl.h>

#include <mzCommon.h>

#ifndef mzVulkan_API
#define mzVulkan_API __declspec(dllimport)
#endif

#define MZ_VULKAN_ASSERT_SUCCESS(expr)                                                                \
    {                                                                                                 \
        VkResult re = (expr);                                                                         \
        while (VK_SUCCESS != re)                                                                      \
        {                                                                                             \
            printf("Error: %s %d\n %s:%d\n", ::mz::vk::vk_result_string(re), re, __FILE__, __LINE__); \
            abort();                                                                                  \
        }                                                                                             \
    }

namespace mz::vk
{

union DescriptorResourceInfo {
    VkDescriptorImageInfo image;
    VkDescriptorBufferInfo buffer;
};

struct MemoryExportInfo
{
    u64 PID;
    HANDLE memory;
    HANDLE sync;
    VkDeviceSize offset;
    VkDeviceSize size;
    VkAccessFlags accessMask;
};

struct ImageCreateInfo
{
    VkExtent2D Extent;
    VkFormat Format;
    VkImageUsageFlags Usage;
    const MemoryExportInfo* Imported = 0;
};

struct mzVulkan_API SVType
{
    enum
    {
        Uint,
        Sint,
        Float,
        Image,
        Struct,
    } tag;

    u32 x = 1; // width
    u32 y = 1; // vecsize
    u32 z = 1; // matsize

    struct Image
    {
        bool depth;
        bool arrayed;
        bool ms;
        bool read;
        bool write;
        u32 sampled;
        VkFormat format;
    } image;

    struct Member
    {
        rc<SVType> type;

        u32 idx;
        u32 size;
        u32 offset;
    };

    std::unordered_map<std::string, Member> members;

    u32 size;
    u32 align;
};

struct mzVulkan_API NamedDSLBinding
{
    uint32_t binding;
    VkDescriptorType descriptorType;
    uint32_t descriptorCount;
    std::string name;
    rc<SVType> type;
};

struct mzVulkan_API ShaderLayout
{
    u32 RTCount;
    u32 PushConstantSize;
    std::map<u32, std::map<u32, NamedDSLBinding>> DescriptorSets;
    std::unordered_map<std::string, glm::uvec2> BindingsByName;
};

mzVulkan_API void ReadInputLayout(View<u8> bin, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes);

mzVulkan_API ShaderLayout GetShaderLayouts(View<u8> bin);
mzVulkan_API bool IsImportable(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage);

mzVulkan_API bool PlatformCloseHandle(HANDLE);
mzVulkan_API HANDLE PlatformDupeHandle(u64 pid, HANDLE);
mzVulkan_API u64 PlatformGetCurrentProcessId();

mzVulkan_API std::string GetLastErrorAsString();

mzVulkan_API std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(VkPhysicalDevice physicalDevice, u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps);
mzVulkan_API void ImageLayoutTransition(VkImage Image,
                                        rc<struct CommandBuffer> Cmd,
                                        VkImageLayout CurrentLayout,
                                        VkImageLayout TargetLayout,
                                        VkAccessFlags srcAccessMask,
                                        VkAccessFlags dstAccessMask,
                                        u32 srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                        u32 dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

mzVulkan_API const char* vk_result_string(VkResult re);
mzVulkan_API const char* descriptor_type_to_string(VkDescriptorType ty);
} // namespace mz::vk
