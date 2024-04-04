/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

// External
#include <vkl.h>

// Framework
//#include <nosCommon.h>
//#include <nosUtil/Logger.h>

#include <memory>
#include <unordered_map>
#include <map>
#include <string>
#include <functional>
#include <variant>
#include <mutex>
#include <array>
#include <optional>
#include <set>
#include <algorithm>

#ifdef nosVulkan_SHARED
#ifdef nosVulkan_EXPORTS
#define nosVulkan_API __declspec(dllexport)
#else
#define nosVulkan_API __declspec(dllimport)
#endif // nosVulkan_EXPORTS
#else
#define nosVulkan_API
#endif // nosVulkan_SHARED

#define NOS_VULKAN_FAILED(expr) (VK_SUCCESS != (expr))
#define NOS_VULKAN_SUCCEEDED(expr) (!NOS_VULKAN_FAILED(expr))

#define MARK_LINE printf("%s:%d\n", __FILE__, __LINE__)

#ifndef NOS_ASSERT
#define NOS_ASSERT(x)                                                                                                   \
	{                                                                                                                  \
		if (!(x))                                                                                                      \
		{                                                                                                              \
			printf("[NOS] Assertion failed at ");                                                                       \
			MARK_LINE;                                                                                                 \
            assert(false);                                                                                             \
		}                                                                                                              \
	}
#endif

#define NOSVK_ASSERT(expr)                                                                                         \
    {                                                                                                             \
        VkResult re = (expr);                                                                                     \
        if (NOS_VULKAN_FAILED(re))                                                                                 \
        {                                                                                                         \
            char errbuf[4096];                                                                                    \
            std::snprintf(errbuf, 4096, "%s %d (%s:%d)", ::nos::vk::vk_result_string(re), re, __FILE__, __LINE__); \
            printf("%s\n", errbuf);                                                                               \
            fflush(stdout);                                                                                       \
            assert(false);                                                                                        \
        }                                                                                                         \
    }

inline bool operator == (VkExtent2D a, VkExtent2D b) {return a.width == b.width && a.height == b.height; }
inline bool operator == (VkExtent3D a, VkExtent3D b) {return a.width == b.width && a.height == b.height && a.depth == b.depth; }

namespace nos::vk
{

template <typename T>
using rc = std::shared_ptr<T>;

template <class T, class... Args>
	requires(std::is_constructible_v<T, Args...>)
rc<T> MakeShared(Args&&... args)
{
	return std::make_shared<T>(std::forward<Args>(args)...);
}

template <class T>
struct SharedFactory : std::enable_shared_from_this<T>
{
	SharedFactory() = default;
	SharedFactory(SharedFactory const&) = delete;

	template <class... Args>
		requires(std::is_constructible_v<T, Args...>)
	static rc<T> New(Args&&... args)
	{
		return MakeShared<T>(std::forward<Args>(args)...);
	}
};


inline void DummyLog(const char* fmt, ...)
{
}

struct Log
{
	void (*D)(const char* fmt, ...) = DummyLog;
	void (*I)(const char* fmt, ...) = DummyLog;
	void (*W)(const char* fmt, ...) = DummyLog;
	void (*E)(const char* fmt, ...) = DummyLog;
};

extern Log GLog;
    

inline void hash_combine(std::size_t& seed) {}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	hash_combine(seed, rest...);
}

template <class T, template <class...> class U>
struct SpecializationOf : std::false_type
{
};

template <template <class...> class U, class... Args>
struct SpecializationOf<U<Args...>, U> : std::true_type
{
};

template <class T, template <class...> class U>
concept spec_of = SpecializationOf<std::remove_cvref_t<T>, U>::value;

template <class T>
concept HasEnabledSharedFromThis = requires(T* t) {
									   {
										   t->shared_from_this()
										   } -> spec_of<std::shared_ptr>;
								   };

template <class T = u64>
struct CircularIndex
{
	T Val;
	T Max;

	explicit CircularIndex(T max) : Val(0), Max(u64(max)) {}

	CircularIndex& operator=(T max)
	{
        Val = 0;
		this->Max = (u64)max;
		return *this;
	}

	u64 operator++() { return Val = (Val + 1) % Max; }

	u64 operator++(int)
	{
		u64 ret = Val % Max;
        Val = (Val + 1) % Max;
		return ret;
	}

	operator u64() const { return Val % Max; }
};

constexpr auto API_VERSION_USED = VK_API_VERSION_1_3;

struct Device;
struct Queue;
struct Allocator;

struct Image;
struct ImageView;
struct Buffer;
struct CommandBuffer;
struct CommandPool;
struct QueryPool;

struct DeviceChild
{
  Device* Vk = 0;
  DeviceChild() = default;
  DeviceChild(Device* Vk)  : Vk(Vk) {}
  Device* GetDevice() const { return Vk; }
  virtual ~DeviceChild() = default;
	virtual vk::Image* AsImage() { return nullptr; }
	virtual vk::Buffer* AsBuffer() { return nullptr; } 
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
static_assert(sizeof(VkDescriptorImageInfo) == sizeof(DescriptorResourceInfo));
static_assert(sizeof(VkDescriptorBufferInfo) == sizeof(DescriptorResourceInfo));

// Might need something like this later if Memory and Sync object are not originating from the same process
/*
struct HandleExportInfo 
{
    u64 PID;
    HANDLE Handle;
    VkExternalMemoryHandleTypeFlagBits Type;
};
*/
bool nosVulkan_API IsYCbCr(VkFormat);

bool nosVulkan_API IsFormatSupportedByDevice(const VkFormat&, const VkPhysicalDevice&);

struct MemoryProperties
{
	bool Mapped = 0;
	bool VRAM = 0;
	bool Download = 0;
	uint32_t Alignment = 0;

    auto operator<=>(const MemoryProperties&) const = default;
};

struct MemoryExportInfo
{
    uint32_t HandleType;
    u64 PID;
    HANDLE Handle;
	uint64_t Offset;
	uint64_t Size;
	uint64_t AllocationSize;
	MemoryProperties MemProps;
};

#if _WIN32
constexpr auto PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
constexpr auto PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

struct BufferCreateInfo
{
    u64 Size = 0;
    VkBufferUsageFlags Usage;
	MemoryProperties MemProps;
    uint32_t ExternalMemoryHandleType = PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE;
    const MemoryExportInfo* Imported = 0;
	int ElementType = 0;
};

struct ImageCreateInfo
{
    VkExtent2D Extent;
    VkFormat Format;
    VkImageUsageFlags Usage;
    VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageCreateFlags Flags = VK_IMAGE_CREATE_ALIAS_BIT;
    uint32_t ExternalMemoryHandleType = PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE;
    const MemoryExportInfo* Imported = 0;
};

struct nosVulkan_API SVType
{
    enum
    {
        Uint,
        Sint,
        Float,
        Image,
        Struct,
        Sampler,
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
    } Img = {};

    struct Sampler
    {

    };

    struct Member
    {
        rc<SVType> Type;
        u32 Idx;
        u32 Size;
        u32 Offset;
    };

    std::string StructName;
    std::unordered_map<std::string, Member> Members;

    u32 Size = 0;
    u32 Alignment = 0;
    u32 ArraySize = 0;
};

}

template<>
struct std::hash<nos::vk::rc<nos::vk::SVType>>
{
    size_t operator()(nos::vk::rc<nos::vk::SVType> const& ty) const
    {
        size_t seed = 0;
        nos::vk::hash_combine(seed,
            ty->Tag, ty->x, ty->y, ty->z,
            ty->Img.Depth, ty->Img.Array, ty->Img.MS, 
            ty->Img.Read, ty->Img.Write, ty->Img.Sampled, ty->Img.Fmt,
            ty->Size, ty->Alignment, ty->ArraySize);

        if (nos::vk::SVType::Struct == ty->Tag)
        {
            nos::vk::hash_combine(seed, ty->StructName);
            for (auto& [n, f] : ty->Members)
                nos::vk::hash_combine(seed, n, f.Type, f.Idx, f.Size, f.Offset);
        }

        return seed;
    }
};

namespace nos::vk
{
enum AccessFlags : int
{
	AccessFlagNone = 0,
	AccessFlagWrite = 1,
	AccessFlagRead = 2,
	AccessFlagReadWrite = 3
};

struct nosVulkan_API NamedDSLBinding
{
    uint32_t Binding;
    VkDescriptorType DescriptorType;
    uint32_t DescriptorCount;
    std::string Name;
    rc<SVType> Type;
    VkShaderStageFlags StageMask;
	AccessFlags Access;
    
    bool SSBO() const
    {
        switch(DescriptorType)
        {
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                return true;
            default: return false;
        }
    }

};

struct nosVulkan_API ShaderLayout
{
    struct Index { u32 set, binding, offset;};
    u32 RTCount;
    u32 PushConstantSize;
    std::map<u32, std::map<u32, NamedDSLBinding>> DescriptorSets;
    std::unordered_map<std::string, Index> BindingsByName;
    ShaderLayout Merge(ShaderLayout const&) const;
};

nosVulkan_API ShaderLayout GetShaderLayouts(std::vector<u8> const& src, VkShaderStageFlags& stage, VkVertexInputBindingDescription& binding, std::vector<VkVertexInputAttributeDescription>& attributes);
nosVulkan_API VkExternalMemoryProperties GetExportProperties(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage, VkExternalMemoryHandleTypeFlagBits Type);
nosVulkan_API bool IsImportable(VkPhysicalDevice PhysicalDevice, VkFormat Format, VkImageUsageFlags Usage, VkExternalMemoryHandleTypeFlagBits Type);

nosVulkan_API bool PlatformCloseHandle(HANDLE);
nosVulkan_API HANDLE PlatformDupeHandle(u64 pid, HANDLE);
nosVulkan_API u64 PlatformGetCurrentProcessId();

nosVulkan_API std::string GetLastErrorAsString();

nosVulkan_API std::pair<u32, VkMemoryType> MemoryTypeIndex(VkPhysicalDevice physicalDevice, u32 memoryTypeBits, VkMemoryPropertyFlags requestedProps);

nosVulkan_API void ImageLayoutTransition(VkImage Image,
                                        rc<CommandBuffer> Cmd,
                                        ImageState Src,
                                        ImageState Dst, VkImageAspectFlags Aspect);
nosVulkan_API void ImageLayoutTransition2(VkImage Image,
                                        rc<CommandBuffer> Cmd,
                                        ImageState Src,
                                        ImageState Dst, VkImageAspectFlags Aspect);

nosVulkan_API const char* vk_result_string(VkResult re);
nosVulkan_API const char* descriptor_type_to_string(VkDescriptorType ty);
} // namespace nos::vk
