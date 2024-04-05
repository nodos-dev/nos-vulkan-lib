/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Common.h"

#include <unordered_set>

namespace nos::vk
{

struct nosVulkan_API QueryPool: SharedFactory<QueryPool>, DeviceChild
{
    VkQueryPool Handle = 0;
    CircularIndex<uint32_t> Queries;
    rc<Buffer> Results;
    const f64 Period = 1;
    std::unordered_map<uint64_t, std::vector<std::chrono::nanoseconds>> ReadyQueries;
    
    QueryPool(Device* device);
    ~QueryPool();
    std::optional<std::chrono::nanoseconds> PerfScope(u64 frames, uint64_t key, CommandBuffer* cmd, std::function<void(CommandBuffer*)>&& f);
	void PerfBegin(uint64_t key, CommandBuffer* cmd);
	std::unordered_map<vk::CommandBuffer*, uint32_t> BeginQueryIdx;
	std::optional<std::chrono::nanoseconds> PerfEnd(uint64_t key, CommandBuffer* cmd, u64 frames);
};
}