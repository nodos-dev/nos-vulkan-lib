
#if defined(__linux__) || defined(__APPLE__)
#include "nosVulkan/Platform.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <cstdio>
#include <dlfcn.h>
#include <pthread.h>

namespace nos::vk
{
    bool PlatformCloseHandle(NOS_HANDLE fd)
	{
		return close(fd) == 0;
	}

	NOS_HANDLE PlatformDupeHandle(u64 pid, NOS_HANDLE handle )
	{
		// Won't work cross-process
		return dup(handle);
	}

	NOS_PID PlatformGetCurrentProcessId()
	{
		return getpid();
	}

	std::string GetLastErrorAsString()
	{
		return strerror(errno);
	}

	void SetThreadName(NOS_HANDLE handle, std::string const& threadName)
	{
		char threadNameCStr[16];
		strncpy(threadNameCStr, threadName.c_str(), 15);
		threadNameCStr[15] = '\0';
#if defined(__linux__)
		pthread_t pthreadHandle = static_cast<pthread_t>(handle);
		int result = pthread_setname_np(pthreadHandle, threadNameCStr);
		if (result != 0)
		{
			fprintf(stderr, "Error setting thread name: %s\n", strerror(result));
		}
#elif defined(__APPLE__)
		(void)handle;
		// macOS can only name the current thread.
#endif
	}

	NOS_HANDLE GetCurrentThread()
	{
		pthread_t thread_id = pthread_self();
		return reinterpret_cast<NOS_HANDLE>(thread_id);
	}

	VkExternalMemoryHandleTypeFlagBits GetPlatformMemoryHandleType()
	{
#if defined(__APPLE__)
		return (VkExternalMemoryHandleTypeFlagBits)PLATFORM_EXTERNAL_MEMORY_HANDLE_TYPE;
#else
		return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
	}
} // namespace nos::vk
#endif
