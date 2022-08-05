
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

	void DeletionQueue::AddFenceForDeletor(std::function<void()>&& deletor, VkFence m_Fence)
	{
		if (!m_Fence)
		{
			return;
		}
		if (m_FencesToWait.contains(deletor))
		{
			m_FencesToWait[deletor].push_back(m_Fence);
		}
		std::vector<VkFence> fences;
		fences.push_back(m_Fence);
		m_FencesToWait.insert({ deletor, fences });
	}

	void DeletionQueue::EnqueueDeletion(std::function<void()>&& deletor, VkFence m_Fence)
	{
		m_Deletors.push_back(deletor);
		if (!m_Fence)
		{
			return;
		}
		if (m_FencesToWait.contains(deletor))
		{
			m_FencesToWait[deletor].push_back(m_Fence);
		}
		std::vector<VkFence> fences;
		fences.push_back(m_Fence);
		m_FencesToWait.insert({ deletor, fences });
	}

	void DeletionQueue::Flush()
	{
		std::function<void()> deletor;
		while (m_Deletors.try_pop_front(&deletor))
		{
			if (m_FencesToWait.contains(deletor))
			{
				auto fences = m_FencesToWait[deletor];
				while (Vk->WaitForFences(fences.size(), fences.data(), VK_TRUE, 10000) != VK_SUCCESS)
				{

				}
			}
			deletor();
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