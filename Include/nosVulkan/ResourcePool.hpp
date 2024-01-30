#pragma once

// Vulkan base
#include <nosVulkan/Common.h>
#include <nosVulkan/Image.h>
#include <nosVulkan/Buffer.h>

// std
#include <unordered_map>
#include <shared_mutex>

namespace nos::vk
{

template <typename ResourceT, typename CreationInfoT,
		  typename CreationInfoHasherT = std::hash<CreationInfoT>,
		  typename CreationInfoEqualsT = std::equal_to<CreationInfoT>,
		  typename ResourceReleasePolicyT = KeepFreeSlots<12>>
class ResourcePool
{
public:
	struct UsedResourceInfo
	{
		std::string Tag;
		CreationInfoT CreationInfo;
		rc<ResourceT> Resource;
	};
	using UsedMap = std::unordered_map<uint64_t, UsedResourceInfo>;
	using CountsPerType = std::unordered_map<CreationInfoT, uint64_t, CreationInfoHasherT, CreationInfoEqualsT>;
	using FreeList = std::vector<rc<ResourceT>>;
	using FreeMap = std::unordered_map<CreationInfoT, FreeList, CreationInfoHasherT, CreationInfoEqualsT>;

	ResourcePool(vk::Device* device) : Device(device)
	{
	}
	
	rc<ResourceT> Get(CreationInfoT const& info, std::string tag)
	{
		std::unique_lock guard(Mutex);
		auto freeIt = Free.find(info);
		if (freeIt == Free.end())
		{
			auto res = typename ResourceT::New(Device, info);
			if (!res)
				return nullptr;
			Used[uint64_t(res->Handle)] = { tag, info, res };
			++Counts[info];
			return res;
		}
		auto& resources = freeIt->second;
		auto res = resources.back();
		resources.pop_back();
		if (resources.empty())
			Free.erase(freeIt);
		Used[uint64_t(res->Handle)] = { tag, info, res };
		return res;
	}

	bool Release(uint64_t handle)
	{
		std::unique_lock guard(Mutex);
		auto usedIt = Used.find(handle);
		if (usedIt == Used.end())
		{
			GLog.W("ResourcePool: %d is already released", handle);
			return false;
		}
		auto [tag, info, res] = usedIt->second;
		Used.erase(usedIt);
		auto& freeList = Free[info];
		if (!ResourceReleasePolicyT::ShouldRelease(freeList.size(), Counts[info]))
			Free[info].push_back(std::move(res));
		else
			--Counts[info];
		return true;
	}

	void Purge()
	{
		std::unique_lock guard(Mutex);
		Free.clear();
	}

	bool IsUsed(uint64_t handle)
	{
		std::shared_lock guard(Mutex);
		return Used.contains(handle);
	}

	ResourceT* FindUsed(uint64_t handle)
	{
		std::shared_lock guard(Mutex);
		auto it = Used.find(handle);
		if (it == Used.end())
			return nullptr;
		return it->second.Resource.get();
	}

	uint64_t GetAvailableResourceCount()
	{
		std::shared_lock guard(Mutex);
		uint64_t ret = 0;
		for (auto& [info, freeList] : Free)
			ret += freeList.size();
		return ret;
	}

	uint64_t GetUsedResourceCount()
	{
		std::shared_lock guard(Mutex);
		return Used.size();
	}

	uint64_t GetTotalMemoryUsage()
	{
		std::shared_lock guard(Mutex);
		return GetAvailableResourceMemoryUsage() + GetUsedResourceMemoryUsage();
	}

	uint64_t GetAvailableResourceMemoryUsage()
	{
		std::shared_lock guard(Mutex);
		uint64_t ret = 0;
		for (auto& [info, freeList] : Free)
			for (auto& free : freeList)
				ret += free->Allocation.GetSize();
		return ret;
	}

	uint64_t GetUsedResourceMemoryUsage()
	{
		std::shared_lock guard(Mutex);
		uint64_t ret = 0;
		for (auto& [handle, info] : Used)
			ret += info.Resource->Allocation.GetSize();
		return ret;
	}
	
	void ChangedUsedResourceTag(uint64_t handle, std::string tag)
	{ 
		std::unique_lock guard(Mutex);
		auto it = Used.find(handle);
		if (it == Used.end())
		{
			GLog.W("Trying to change the tag of an invalid resource.");
			return;
		}
		it->second.Tag = std::move(tag);
	}

	UsedMap GetUsed() { std::shared_lock guard(Mutex); return Used; }
	FreeMap GetFree() { std::shared_lock guard(Mutex); return Free; }
protected:
	vk::Device* Device;
	UsedMap Used;
	FreeMap Free;
	CountsPerType Counts;
	std::shared_mutex Mutex;
};

template <size_t FreeSlotsPerCreationInfo>
struct KeepFreeSlots
{
	static bool ShouldRelease(size_t currentlyFree, size_t allocatedCount)
	{
		if (currentlyFree < FreeSlotsPerCreationInfo)
			return false;
		return true;
	}
};

template <size_t MinCount, float MaxLoadFactor>
struct MaintainLoadFactor
{
	static bool ShouldRelease(size_t currentlyFree, size_t allocatedCount)
	{
		if (currentlyFree < MinCount || (allocatedCount / static_cast<float>(allocatedCount - currentlyFree)) < MaxLoadFactor)
			return false;
		return true;
	}
};

// TODO: Move below to ResourceManager.cpp after if/when ResourcePool is fully generic
namespace detail
{
// Since some of the fields of createinfo structs are not used, hashers and equality checks are implemented here instead of nosVulkan.
struct ImageCreateInfoHasher
{
	size_t operator()(vk::ImageCreateInfo const& info) const
	{
		size_t result = 0;
		vk::hash_combine(result, info.Extent.width, info.Extent.height, info.Format, info.Usage, info.Samples, info.Tiling, info.Flags, info.ExternalMemoryHandleType);
		return result;
	}
};

struct ImageCreateInfoEquals
{
	bool operator()(vk::ImageCreateInfo const& l, vk::ImageCreateInfo const& r) const
	{
		return l.Extent == r.Extent && l.Format == r.Format && l.Usage == r.Usage && l.Samples == r.
			   Samples && l.Tiling == r.Tiling && l.Flags == r.Flags && l.ExternalMemoryHandleType == r.ExternalMemoryHandleType;
	}
};

struct BufferCreateInfoHasher
{
	size_t operator()(vk::BufferCreateInfo const& info) const
	{
		size_t result = 0;
		vk::hash_combine(result, info.Size, info.Mapped, info.VRAM, info.Usage, info.ExternalMemoryHandleType);
		return result;
	}
};

struct BufferCreateInfoEquals
{
	bool operator()(vk::BufferCreateInfo const& l, vk::BufferCreateInfo const& r) const
	{
		return l.Size == r.Size && l.Mapped == r.Mapped && l.VRAM == r.VRAM && l.Usage == r.Usage && l.ExternalMemoryHandleType == r.ExternalMemoryHandleType;
	}
};
} // namespace detail

using ImagePool = ResourcePool<vk::Image, vk::ImageCreateInfo, detail::ImageCreateInfoHasher, detail::ImageCreateInfoEquals, MaintainLoadFactor<12, 1.5>>;
using BufferPool = ResourcePool<vk::Buffer, vk::BufferCreateInfo, detail::BufferCreateInfoHasher, detail::BufferCreateInfoEquals, MaintainLoadFactor<12, 1.5>>;

}
