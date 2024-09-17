/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Common.h"

#include <unordered_set>

namespace nos::vk
{
struct nosVulkan_API QueryResult
{
    uint64_t Timestamp;
    uint64_t Duration;
};

struct nosVulkan_API QueryPool: SharedFactory<QueryPool>, DeviceChild
{
    VkQueryPool Handle = 0;
    CircularIndex<uint32_t> Queries;
    rc<Buffer> Results;
    const f64 Period = 1;
    std::unordered_map<uint64_t, std::vector<std::chrono::nanoseconds>> ReadyQueries;
    
    QueryPool(Device* device);
    ~QueryPool();
	void PerfBegin(uint64_t key, CommandBuffer* cmd);
	std::unordered_map<uint64_t, uint32_t> BeginQueryIdx;
	std::optional<std::chrono::nanoseconds> PerfEnd(uint64_t key, CommandBuffer* cmd, u64 frames, std::function<void(QueryResult)> endCallback);
};
}


