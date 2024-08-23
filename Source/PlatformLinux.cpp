
#ifdef __linux__
#include "nosVulkan/Platform.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <cstdio>
#include <dlfcn.h>

namespace nos::vk
{
    bool PlatformCloseHandle(int fd)
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
} // namespace nos::vk
#endif
