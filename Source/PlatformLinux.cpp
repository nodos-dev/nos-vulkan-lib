
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

	NOS_HANDLE PlatformDupeHandle(u64 pid, NOS_HANDLE)
	{
		return dup(pid); // Returns a new file descriptor or -1 on error
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
