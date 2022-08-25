#pragma once

#include <vkl.h>
#include <mzCommon.h>

#ifndef mzVulkan_API
#define mzVulkan_API __declspec(dllimport)
#endif
#define MZ_VULKAN_FAILED(expr) (VK_SUCCESS != (expr))
#define MZ_VULKAN_SUCCEEDED(expr) (!MZ_VULKAN_FAILED(expr))

#define MZ_VULKAN_ASSERT_SUCCESS(expr)                                                                                 \
    {                                                                                                                  \
        VkResult re = (expr);                                                                                          \
        if (MZ_VULKAN_FAILED(re))                                                                                      \
        {                                                                                                              \
            printf("Error: %s %d\n %s:%d\n", ::mz::vk::vk_result_string(re), re, __FILE__, __LINE__);                  \
            abort();                                                                                                   \
        }                                                                                                              \
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
  virtual ~DeviceChild() = default;
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
bool mzVulkan_API IsYCbCr(VkFormat);

struct MemoryExportInfo
{
    u64 PID;
    HANDLE Memory;
    VkExternalMemoryHandleTypeFlagBits Type;
    VkDeviceSize Offset;
};

struct BufferCreateInfo
{
    u32 Size : 31 = 0;
    u32 VRAM : 1 = 0;
    VkBufferUsageFlags Usage;
    VkExternalMemoryHandleTypeFlagBits Type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    void* Data = 0;
    const MemoryExportInfo* Imported = 0;
};

struct ImageCreateInfo
{
    VkExtent2D Extent;
    VkFormat Format;
    VkImageUsageFlags Usage;
    VkFilter Filtering = VK_FILTER_LINEAR;
    VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageCreateFlags Flags = VK_IMAGE_CREATE_ALIAS_BIT;
    VkExternalMemoryHandleTypeFlagBits Type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
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
    struct Index { u32 set, binding, offset; };
    u32 RTCount;
    u32 PushConstantSize;
    std::map<u32, std::map<u32, NamedDSLBinding>> DescriptorSets;
    std::unordered_map<std::string, Index> BindingsByName;
    ShaderLayout Merge(ShaderLayout const&) const;
};

mzVulkan_API ShaderLayout GetShaderLayouts(View<u8> src, VkShaderStageFlags& stage, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes);
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
mzVulkan_API void ImageLayoutTransition2(VkImage Image,
                                        rc<CommandBuffer> Cmd,
                                        ImageState Src,
                                        ImageState Dst);

mzVulkan_API const char* vk_result_string(VkResult re);
mzVulkan_API const char* descriptor_type_to_string(VkDescriptorType ty);
} // namespace mz::vk
