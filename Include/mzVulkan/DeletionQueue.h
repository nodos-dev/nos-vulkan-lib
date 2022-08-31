#pragma once

#include "Common.h"

#include <mzUtil/Thread.h>
#include <mzUtil/ThreadSafeQueue.h>

namespace mz::vk
{
	struct MemoryBlock;
	struct Device;

	struct mzVulkan_API DeletionQueue : SharedFactory<Allocator>, 
		DeviceChild, 
		ConsumerThread<std::pair<std::function<void()>, std::vector<VkFence>>>
	{
		DeletionQueue(Device* Vk);
		
		
		void EnqueueDeletion(std::function<void()>&& function, std::vector<VkFence> m_Fences);
		void EnqueueDeletion(std::function<void()>&& function, VkFence m_Fence);
		

		protected:
			void Consume(const std::pair<std::function<void()>, std::vector<VkFence>>& pair) ;
			
	};


}