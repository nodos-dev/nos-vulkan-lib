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
		
		
		void EnqueueDeletion(std::function<void()>&& function, std::vector<VkFence> m_Fences);
		void EnqueueDeletion(std::function<void()>&& function, VkFence m_Fence);
		void Flush();
		
		void Run() override;

		private:
			mz::ThreadSafeQueue < std::pair<std::function<void()>, std::vector<VkFence>> > m_Deletors;

			
	};


}