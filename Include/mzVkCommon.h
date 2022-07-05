#pragma once

#include "vulkan/vulkan_core.h"
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

struct Device;
struct Queue;
struct Allocator;

struct Image;
struct ImageView;
struct Buffer;
struct CommandBuffer;
struct CommandPool;

struct DeviceChild
{
  Device* Vk = 0;
  DeviceChild() = default;
  DeviceChild(Device* Vk)  : Vk(Vk) {}
  Device* GetDevice() { return Vk; }
};

struct ImageState
{
    VkPipelineStageFlags2 StageMask;
    VkAccessFlags2 AccessMask;
    VkImageLayout Layout;
};

union DescriptorResourceInfo {
    VkDescriptorImageInfo Image;
    VkDescriptorBufferInfo Buffer;
};

// Might need something like this later if Memory and Sync object are not originating from the same process
/*
struct HandleExportInfo 
{
    u64 PID;
    HANDLE Handle;
    VkExternalMemoryHandleTypeFlagBits Type;
};
*/

struct MemoryExportInfo
{
    u64 PID;
    HANDLE Memory;
    VkExternalMemoryHandleTypeFlagBits Type;
    VkDeviceSize Offset;
};

struct ImageCreateInfo
{
    VkExtent2D Extent;
    VkFormat Format;
    VkImageUsageFlags Usage;
    VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageCreateFlags Flags = VK_IMAGE_CREATE_ALIAS_BIT;
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
    } Tag;

    u32 x = 1; // width
    u32 y = 1; // vecsize
    u32 z = 1; // matsize

    struct Image
    {
        bool Depth;
        bool Array;
        bool MS;
        bool Read;
        bool Write;
        u32 Sampled;
        VkFormat Fmt;
    } Img;

    struct Member
    {
        rc<SVType> Type;
        u32 Idx;
        u32 Size;
        u32 Offset;
    };

    std::unordered_map<std::string, Member> Members;

    u32 Size;
    u32 Alignment;
};

struct mzVulkan_API NamedDSLBinding
{
    uint32_t Binding;
    VkDescriptorType DescriptorType;
    uint32_t DescriptorCount;
    std::string Name;
    rc<SVType> Type;
    VkShaderStageFlags StageMask;
};

struct mzVulkan_API ShaderLayout
{
    struct Index { u32 set; u32 binding; };
    u32 RTCount;
    u32 PushConstantSize;
    std::map<u32, std::map<u32, NamedDSLBinding>> DescriptorSets;
    std::unordered_map<std::string, Index> BindingsByName;
};

mzVulkan_API void ReadInputLayout(View<u8> bin, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes);

mzVulkan_API ShaderLayout GetShaderLayouts(View<u8> bin);

mzVulkan_API VkExternalMemoryProperties GetExportProperties(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage, VkExternalMemoryHandleTypeFlagBits Type);
mzVulkan_API bool IsImportable(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage, VkExternalMemoryHandleTypeFlagBits Type);

mzVulkan_API bool PlatformCloseHandle(HANDLE);
mzVulkan_API HANDLE PlatformDupeHandle(u64 pid, HANDLE);
mzVulkan_API u64 PlatformGetCurrentProcessId();

mzVulkan_API std::string GetLastErrorAsString();

mzVulkan_API std::pair<u32, VkMemoryPropertyFlags> MemoryTypeIndex(VkPhysicalDevice physicalDevice, u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps);

mzVulkan_API void ImageLayoutTransition(VkImage Image,
                                        rc<CommandBuffer> Cmd,
                                        ImageState Src,
                                        ImageState Dst);

mzVulkan_API const char* vk_result_string(VkResult re);
mzVulkan_API const char* descriptor_type_to_string(VkDescriptorType ty);
} // namespace mz::vk
