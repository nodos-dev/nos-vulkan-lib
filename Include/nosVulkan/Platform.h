#pragma once

#if _WIN32

#define WIN32_ASSERT(expr)                                                                                        \
    if (!(expr))                                                                                                  \
    {                                                                                                             \
        char errbuf[1024];                                                                                        \
        std::snprintf(errbuf, 1024, "%s\t(%s:%d)", ::nos::vk::GetLastErrorAsString().c_str(), __FILE__, __LINE__); \
		assert(false);                                                                                            \
    }
#endif

#include "nosVulkan/Common.h"

namespace nos::vk
{

nosVulkan_API bool PlatformCloseHandle(NOS_HANDLE);
nosVulkan_API NOS_HANDLE PlatformDupeHandle(u64 pid, NOS_HANDLE);
nosVulkan_API NOS_PID PlatformGetCurrentProcessId();
nosVulkan_API std::string GetLastErrorAsString();

} // namespace nos::vk
