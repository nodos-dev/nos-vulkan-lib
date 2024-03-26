// Copyright MediaZ AS. All Rights Reserved.

#include "nosVulkan/QueryPool.h"
#include "nosVulkan/Device.h"
#include "nosVulkan/Command.h"
#include "nosVulkan/Buffer.h"
#include <chrono>
#include <numeric>

namespace nos::vk
{

static f64 GetPeriod(Device* Vk)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(Vk->PhysicalDevice, &props);
    return props.limits.timestampPeriod;
}

QueryPool::QueryPool(Device* Vk) : DeviceChild(Vk), Results(Buffer::New(Vk, BufferCreateInfo {
        .Size = (1<<16)*8,
        .Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .MemProps = { .Mapped = true, .Download = true },
    })), Period(GetPeriod(Vk)), Queries(1<<16)
{
    VkQueryPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 1<<16,
    };
    NOSVK_ASSERT(Vk->CreateQueryPool(&info, 0, &Handle));
    Vk->ResetQueryPool(Handle, 0, 1<<16);
}

std::optional<std::chrono::nanoseconds>  QueryPool::PerfScope(u64 frames,std::string const& key, rc<CommandBuffer> Cmd, std::function<void(rc<CommandBuffer>)>&& f)
{
    const u64 idx = Queries;
    Cmd->WriteTimestamp(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, Handle, Queries++);
    f(Cmd);
    Cmd->WriteTimestamp(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, Handle, Queries++);
    Cmd->CopyQueryPoolResults(Handle, idx, 2, Results->Handle, idx * 8, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    Cmd->ResetQueryPool(Handle, idx, 2);
    Cmd->Callbacks.push_back([this, idx, key] {
        u64* ptr = (u64*)Results->Map();
        ReadyQueries[key].push_back(std::chrono::nanoseconds(u64((ptr[idx+1]-ptr[idx]) * Period + 0.5)));
        ptr[idx+1] = 0;
        ptr[idx+0] = 0;
    });
    
    auto& q = ReadyQueries[key];
    if(q.size() >= frames)
    {
        const auto avg = std::accumulate(q.begin(), q.end(), std::chrono::nanoseconds(0)) / q.size();
        q.clear();
        return avg;
    }
    return {};
}

}