// Copyright MediaZ AS. All Rights Reserved.


// External
#include <vulkan/vulkan_core.h>

// Framework
#include <mzDefines.h>

// mzVulkan
#include "mzVulkan/Common.h"
#include "mzVulkan/Device.h"
#include "mzVulkan/DeletionQueue.h"


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
		m_Queue.push_back({ deletor, fences });
	}

	void DeletionQueue::EnqueueDeletion(std::function<void()>&& deletor, std::vector<VkFence> m_Fences)
	{
		m_Queue.push_back({ deletor, m_Fences });
	}

	void DeletionQueue::Consume(const std::pair<std::function<void()>, std::vector<VkFence>>& pair)
	{
		auto fences = pair.second;
		if (!fences.empty())
		{
			while (Vk->WaitForFences(fences.size(), fences.data(), VK_TRUE, 10000) != VK_SUCCESS)
			{
				//wait until fence is successfull
			}
		}
		pair.first();
	}

}