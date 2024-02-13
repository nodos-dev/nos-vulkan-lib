/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Common.h"

namespace nos::vk
{

struct nosVulkan_API QueryPool: SharedFactory<QueryPool>, DeviceChild
{
    VkQueryPool Handle = 0;
    CircularIndex<u64> Queries;
    rc<Buffer> Results;
    const f64 Period = 1;
    std::unordered_map<std::string, std::vector<std::chrono::nanoseconds>> ReadyQueries;
    
    QueryPool(Device* Vk);
    std::optional<std::chrono::nanoseconds> PerfScope(u64 frames, std::string const& key, rc<CommandBuffer> Cmd, std::function<void(rc<CommandBuffer>)>&& f);
};
}