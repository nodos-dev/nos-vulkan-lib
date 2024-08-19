
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

	int PlatformDupeHandle(int fd)
	{
		return dup(fd); // Returns a new file descriptor or -1 on error
	}

	pid_t PlatformGetCurrentProcessId()
	{
		return getpid();
	}

	std::string GetLastErrorAsString()
	{
		return strerror(errno);
	}
} // namespace nos::vk
#endif
