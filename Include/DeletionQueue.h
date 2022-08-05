#pragma once

#include <mzVkCommon.h>
#include <mzUtil/Thread.h>
#include <mzUtil/ThreadSafeQueue.h>

namespace mz::vk
{
	struct MemoryBlock;
	struct Device;

	struct mzVulkan_API DeletionQueue : SharedFactory<Allocator>, DeviceChild, Thread
	{
		DeletionQueue(Device* Vk);
		
		void AddFenceForDeletor(std::function<void()>&& deletor, VkFence m_Fence);
		void EnqueueDeletion(std::function<void()>&& function, VkFence m_Fence);
		void Flush();
		
		void Run() override;

		private:
			mz::ThreadSafeQueue<std::function<void()>> m_Deletors;
			//std::deque<std::function<void()>> m_Deletors;
			std::map<std::function<void()>, std::vector<VkFence>> m_FencesToWait;
	};


}