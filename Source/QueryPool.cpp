#include "mzVulkan/QueryPool.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/Command.h"

namespace mz::vk
{

QueryPool::QueryPool(Device* Vk) : DeviceChild(Vk)
{
    VkQueryPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 1<<16,
    };
    MZ_VULKAN_ASSERT_SUCCESS(Vk->CreateQueryPool(&info, 0, &Handle));
}

void QueryPool::Begin(rc<CommandBuffer> Cmd)
{
    this->Cmd = Cmd;
}

void QueryPool::End()
{
    Cmd->Callbacks.push_back([this] {
        std::vector<u64> buf;
        buf.resize(Queries);
        MZ_VULKAN_ASSERT_SUCCESS(GetDevice()->GetQueryPoolResults(Handle, 0, Queries, buf.size() * 8, buf.data(), 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        Results.insert(Results.end(), buf.begin(), buf.end());
        GetDevice()->ResetQueryPool(Handle, 0, 1<<16);
        Queries = 0;
    });
}

void QueryPool::Timestamp()
{
    Cmd->WriteTimestamp(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, Handle, Queries++);
}

}