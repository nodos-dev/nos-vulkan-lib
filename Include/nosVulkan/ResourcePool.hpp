// Copyright MediaZ AS. All Rights Reserved.

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
		  typename CreationInfoEqualsT = std::equal_to<CreationInfoT>>
class ResourcePool
{
public:
	struct UsedResourceInfo
	{
		std::string Tag;
		CreationInfoT CreationInfo;
		rc<ResourceT> Resource;
	};
	template <typename T>
	using MapWithCreationInfoAsKey = std::unordered_map<CreationInfoT, T, CreationInfoHasherT, CreationInfoEqualsT>;

	using UsedMap = std::unordered_map<uint64_t, UsedResourceInfo>;
	using CountsPerType = std::unordered_map<CreationInfoT, uint64_t, CreationInfoHasherT, CreationInfoEqualsT>;
	using FreeList = std::list<rc<ResourceT>>;
	using FreeMap = MapWithCreationInfoAsKey<FreeList>;

	ResourcePool(vk::Device* device)
		: Device(device) {}

	ResourcePool(vk::Device* device,
	             size_t minCountPerType,
	             float maxLoadFactorPerType,
	             float maxFree2BudgetRatio)
		: Device(device), MinCountPerType(minCountPerType),
		  MaxLoadFactorPerType(maxLoadFactorPerType),
		  MaxFree2BudgetRatio(maxFree2BudgetRatio) {}
	
	rc<ResourceT> Get(CreationInfoT const& info, std::string tag)
	{
		std::unique_lock guard(Mutex);
		auto freeIt = Free.find(info);
		if (freeIt == Free.end() || freeIt->second.empty())
		{
			auto res = typename ResourceT::New(Device, info);
			if (!res)
				return nullptr;
			Used[uint64_t(res->Handle)] = { tag, info, res };
			++Counts[info];
			CheckAndClean();
			return res;
		}
		auto& resources = freeIt->second;
		auto res = resources.back();
		resources.pop_back();
		if (resources.empty())
			Free.erase(freeIt);
		Used[uint64_t(res->Handle)] = { tag, info, res };
		CheckAndClean();
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
		Free[info].push_back(std::move(res));
		CheckAndClean();
		return true;
	}

	virtual bool ShouldRelease(CreationInfoT const& info)
	{
		auto mem = Device->GetCurrentMemoryUsage();
		auto freeUsage = GetAvailableResourceMemoryUsage();
		float c = freeUsage / float(mem.Budget);
		if (c > MaxFree2BudgetRatio)
			return true;
		const auto currentlyFree = Free[info].size();
		const auto allocatedCount = Counts[info];
		const auto loadFactor = allocatedCount / static_cast<float>(allocatedCount - currentlyFree);
		if (currentlyFree < MinCountPerType || loadFactor < MaxLoadFactorPerType)
			return false;
		return true;
	}
	
	virtual void CheckAndClean()
	{
		for (auto& [info, freeList] : Free)
		{
			while (!freeList.empty() && ShouldRelease(info))
			{
				freeList.pop_front();
				--Counts[info];
			}
		}
		for (auto it = Free.begin(); it != Free.end();)
		{
			if (it->second.empty())
				it = Free.erase(it);
			else
				++it;
		}
	}

	void GarbageCollect()
	{
		std::unique_lock guard(Mutex);
		Free.clear();
		Counts.clear();
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

	/// Not thread safe.
	uint64_t GetAvailableResourceMemoryUsage()
	{
		uint64_t ret = 0;
		for (auto& [info, freeList] : Free)
			for (auto& free : freeList)
				ret += free->Allocation.GetSize();
		return ret;
	}

	/// Not thread safe.
	uint64_t GetUsedResourceMemoryUsage()
	{
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
	std::shared_mutex Mutex{};

	// Options
	size_t MinCountPerType = 12;
	float MaxLoadFactorPerType = 1.5f; // Per type, All / Free.
	float MaxFree2BudgetRatio = .5f; // Memory, Free resources / Remaining budget.
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

using ImagePool = ResourcePool<vk::Image, vk::ImageCreateInfo, detail::ImageCreateInfoHasher, detail::ImageCreateInfoEquals>;
using BufferPool = ResourcePool<vk::Buffer, vk::BufferCreateInfo, detail::BufferCreateInfoHasher, detail::BufferCreateInfoEquals>;

}
