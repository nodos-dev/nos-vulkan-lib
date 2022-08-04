#pragma once

#include <mzVkCommon.h>

namespace mz::vk
{
	struct MemoryBlock;
	struct Device;

	struct mzVulkan_API DeletionQueue : SharedFactory<Allocator>, DeviceChild
	{
		DeletionQueue(Device* Vk);
		DeletionQueue(Device* Vk, VkFence m_Fence);

		void AddFenceToWait(VkFence m_Fence);
		void EnqueueDeletion(std::function<void()>&& function);
		VkResult Flush();

		private:
			std::deque<std::function<void()>> m_Deletors;
			std::vector<VkFence> m_FencesToWait;
	};


}