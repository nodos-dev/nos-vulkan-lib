
#ifdef __linux__
#include "nosVulkan/Platform.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <cstdio>
#include <dlfcn.h>

namespace nos::vk
{
    bool PlatformCloseHandle(NOS_HANDLE fd)
	{
		return close(fd) == 0;
	}

	NOS_HANDLE PlatformDupeHandle(u64 pid, NOS_HANDLE handle )
	{
		int pidfdf_res = syscall(SYS_pidfd_getfd, pid, handle, 0);
		if(pidfdf_res == -1) return NULL;
		return pidfdf_res;	
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
		//From:
		//https://man7.org/linux/man-pages/man3/pthread_setname_np.3.html
		/*
		The thread name is a
       	meaningful C language string, whose length is restricted to 16
       	characters, including the terminating null byte ('\0').
		*/
		char threadNameCStr[16];
		strncpy(threadNameCStr, threadName.c_str(), 15);
		threadNameCStr[15] = '\0';

		pthread_t pthreadHandle = static_cast<pthread_t>(handle);
		int result = pthread_setname_np(pthreadHandle, threadNameCStr);
		if (result != 0)
		{
			fprintf(stderr, "Error setting thread name: %s\n", strerror(result));
		}
	}

	NOS_HANDLE GetCurrentThread()
	{
		pthread_t thread_id = pthread_self();
		return static_cast<NOS_HANDLE>(thread_id);
	}
} // namespace nos::vk
#endif
