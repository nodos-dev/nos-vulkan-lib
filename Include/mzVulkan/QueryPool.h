#pragma once

#include "Common.h"

namespace mz::vk
{

struct mzVulkan_API QueryPool: SharedFactory<QueryPool>, DeviceChild
{
    VkQueryPool Handle = 0;
    std::atomic_uint64_t Queries = 0;
    rc<CommandBuffer> Cmd;
    std::vector<u64> Results;
    QueryPool(Device* Vk);
    void Begin(rc<CommandBuffer>);
    void End();
    void Timestamp();
};
}