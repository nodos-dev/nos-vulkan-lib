
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

	DeletionQueue::DeletionQueue(Device* Vk, VkFence m_Fence)
		:DeviceChild(Vk)
	{
		m_FencesToWait.push_back(m_Fence);
	}

	void DeletionQueue::AddFenceToWait(VkFence m_Fence)
	{
		m_FencesToWait.push_back(m_Fence);
	}

	void DeletionQueue::EnqueueDeletion(std::function<void()>&& deletor)
	{
		m_Deletors.push_back(deletor);
	}

	VkResult DeletionQueue::Flush()
	{
		if (!m_FencesToWait.empty())
		{
			auto result = Vk->WaitForFences(m_FencesToWait.size(), m_FencesToWait.data(), VK_TRUE, 10000);
			if (result != VK_SUCCESS)
			{
				return result;
			}
		}

		for (auto deletor = m_Deletors.rbegin(); deletor != m_Deletors.rend(); ++deletor)
		{
			(*deletor)();
		}

		m_Deletors.clear();

		return VK_SUCCESS;
	}

}