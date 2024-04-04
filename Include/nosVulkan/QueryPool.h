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
    std::unordered_map<std::string, std::vector<std::chrono::nanoseconds>> ReadyQueries;
	std::unordered_map<std::string, int64_t> BeginEnd;
    
    QueryPool(Device* device);
    ~QueryPool();
    std::optional<std::chrono::nanoseconds> PerfScope(u64 frames, std::string const& key, CommandBuffer* cmd, std::function<void(CommandBuffer*)>&& f);
	void PerfBegin(std::string const& key, CommandBuffer* cmd);
	uint32_t LastBeginQueryIdx;
	std::optional<std::chrono::nanoseconds> PerfEnd(std::string const& key, CommandBuffer* cmd, u64 frames);
};
}