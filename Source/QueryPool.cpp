// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

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

QueryPool::~QueryPool() 
{
	Vk->DestroyQueryPool(Handle, 0);
}

void QueryPool::PerfBegin(uint64_t key, CommandBuffer* cmd)
{
	assert(!BeginQueryIdx.contains(key));
	BeginQueryIdx[key] = Queries;
	cmd->WriteTimestamp(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, Handle, BeginQueryIdx[key]);
	++Queries;
}

std::optional<std::chrono::nanoseconds> QueryPool::PerfEnd(uint64_t key, CommandBuffer* cmd, u64 frames, std::function<void(QueryResult)> callbackFunc)
{
	auto endQuery = Queries++;
	auto beginQuery = BeginQueryIdx[key];
	cmd->WriteTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Handle, endQuery);
	cmd->CopyQueryPoolResults(Handle, beginQuery, 1, Results->Handle, beginQuery * 8, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	cmd->CopyQueryPoolResults(Handle, endQuery, 1, Results->Handle, endQuery * 8, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	cmd->ResetQueryPool(Handle, beginQuery, 1);
	cmd->ResetQueryPool(Handle, endQuery, 1);
	cmd->Callbacks.push_back([this, beginQuery, endQuery, key, callbackFunc = std::move(callbackFunc)] {
		u64* ptr = (u64*)Results->Map();
		u64 start = u64(ptr[beginQuery]) * Period + 0.5;
		u64 end = u64(ptr[endQuery]) * Period + 0.5;
		QueryResult result = { .Timestamp = start, .Duration = end - start };
		ReadyQueries[key].push_back(std::chrono::nanoseconds(end-start));
		ptr[endQuery] = 0;
		ptr[beginQuery] = 0;
		if (callbackFunc)
			callbackFunc(result);
	});

	BeginQueryIdx.erase(key);

	auto& q = ReadyQueries[key];
	if(q.size() >= frames)
	{
		const auto avg = std::accumulate(q.begin(), q.end(), std::chrono::nanoseconds(0)) / q.size();
		q.clear();
		return avg;
	}
	return std::nullopt;
}

}