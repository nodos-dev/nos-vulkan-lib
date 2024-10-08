// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#pragma once

// Vulkan base
#include <nosVulkan/Common.h>
#include <nosVulkan/Image.h>
#include <nosVulkan/Buffer.h>

// std
#include <unordered_map>
#include <shared_mutex>
#include <list>
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

	ResourcePool(vk::Device* device, std::chrono::milliseconds maxUnusedTime)
		: Device(device), MaxUnusedTime(maxUnusedTime) {}
	
	rc<ResourceT> Get(CreationInfoT const& info, std::string tag)
	{
		std::unique_lock guard(Mutex);
		auto freeIt = Free.find(info);
		if (freeIt == Free.end() || freeIt->second.empty())
		{
			auto res = ResourceT::New(Device, info); //was typename
			if (!res)
				return nullptr;
			Used[uint64_t(res->Handle)] = { tag, info, res };
			UsedResourceMemoryUsage += res->Size;
			CheckAndClean();
			return res;
		}
		auto& freeList = freeIt->second;
		auto res = freeList.back();
		freeList.pop_back();
		ReadyResourceMemoryUsage -= res->Size;
		if (freeList.empty())
			Free.erase(freeIt);
		Used[uint64_t(res->Handle)] = { tag, info, res };
		UsedResourceMemoryUsage += res->Size;
		CheckAndClean();
		return res;
	}

	bool Release(uint64_t handle)
	{
		std::unique_lock guard(Mutex);
		auto usedIt = Used.find(handle);
		if (usedIt == Used.end())
			return false;
		auto [tag, info, res] = usedIt->second;
		auto size = res->Size;
		Used.erase(usedIt);
		UsedResourceMemoryUsage -= size;
		Free[info].push_back(std::move(res));
		ReleaseTime[info] = std::chrono::steady_clock::now();
		ReadyResourceMemoryUsage += size;
		CheckAndClean();
		return true;
	}

	virtual void CheckAndClean()
	{
		auto now = std::chrono::steady_clock::now();
		for (auto it = ReleaseTime.begin(); it != ReleaseTime.end();)
		{
			auto& [info, released] = *it;
			auto fit = Free.find(info);
			if (fit != Free.end() && now - released > MaxUnusedTime)
			{
				auto& freeList = fit->second;
				while (!freeList.empty())
				{
					ReadyResourceMemoryUsage -= freeList.front()->Size;
					freeList.pop_front();
				}
				Free.erase(fit);
				it = ReleaseTime.erase(it);
			}
			else
				++it;
		}
	}

	void GarbageCollect()
	{
		std::unique_lock guard(Mutex);
		Free.clear();
		ReadyResourceMemoryUsage = 0;
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
		return GetReadyResourceMemoryUsage() + GetUsedResourceMemoryUsage();
	}

	/// Not thread safe.
	uint64_t GetReadyResourceMemoryUsage()
	{
		return ReadyResourceMemoryUsage;
	}

	/// Not thread safe.
	uint64_t GetUsedResourceMemoryUsage()
	{
		return UsedResourceMemoryUsage;
	}
	
	void SetUsedResourceTag(uint64_t handle, std::string tag)
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

	void SetMaxUnusedTime(std::chrono::milliseconds time)
	{
		std::unique_lock guard(Mutex); 
		MaxUnusedTime = time;
	}
protected:
	vk::Device* Device;
	UsedMap Used;
	FreeMap Free;
	CountsPerType AllocatedCounts;
	std::shared_mutex Mutex{};
	std::unordered_map<CreationInfoT, std::chrono::steady_clock::time_point, CreationInfoHasherT, CreationInfoEqualsT> ReleaseTime;

	// Options
	std::chrono::milliseconds MaxUnusedTime;

	// Runtime memory usage info
	uint64_t UsedResourceMemoryUsage = 0;
	uint64_t ReadyResourceMemoryUsage = 0;

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
		vk::hash_combine(result, info.Size, info.MemProps.Mapped, info.MemProps.VRAM, info.MemProps.Download, info.Usage, info.ExternalMemoryHandleType);
		return result;
	}
};

struct BufferCreateInfoEquals
{
	bool operator()(vk::BufferCreateInfo const& l, vk::BufferCreateInfo const& r) const
	{
		return l.Size == r.Size && l.MemProps == r.MemProps && l.Usage == r.Usage && l.ExternalMemoryHandleType == r.ExternalMemoryHandleType;
	}
};
} // namespace detail

using ImagePool = ResourcePool<vk::Image, vk::ImageCreateInfo, detail::ImageCreateInfoHasher, detail::ImageCreateInfoEquals>;
using BufferPool = ResourcePool<vk::Buffer, vk::BufferCreateInfo, detail::BufferCreateInfoHasher, detail::BufferCreateInfoEquals>;

}
