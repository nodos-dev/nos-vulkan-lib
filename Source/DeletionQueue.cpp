
#include "mzDefines.h"
#include "mzVkCommon.h"
#include "vulkan/vulkan_core.h"
#include <Device.h>

#include <DeletionQueue.h>


namespace mz::vk
{
	DeletionQueue::DeletionQueue(Device* Vk)
		:DeviceChild(Vk)
	{
	}

	void DeletionQueue::EnqueueDeletion(std::function<void()>&& deletor, VkFence m_Fence)
	{
		std::vector<VkFence> fences;
		fences.push_back(m_Fence);
		m_Deletors.push_back({ deletor, fences });
	}

	void DeletionQueue::EnqueueDeletion(std::function<void()>&& deletor, std::vector<VkFence> m_Fences)
	{
		m_Deletors.push_back({ deletor, m_Fences });
	}

	void DeletionQueue::Flush()
	{
		std::pair<std::function<void()>, std::vector<VkFence>> pair;
		while (m_Deletors.try_pop_front(&pair))
		{
			auto fences = pair.second;
			while (Vk->WaitForFences(fences.size(), fences.data(), VK_TRUE, 10000) != VK_SUCCESS)
			{

			}
			pair.first();
		}
	}

	void DeletionQueue::Run()
	{
		while (m_ShouldRun)
		{
			Flush();
		}
	}

}