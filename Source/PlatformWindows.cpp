
#if defined(_WIN32)
#include "nosVulkan/Platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
namespace nos::vk
{
	bool PlatformCloseHandle(NOS_HANDLE handle)
	{
		DWORD flags;
		return /*GetHandleInformation(handle, &flags) && */CloseHandle(handle);
	}
	
	HANDLE PlatformDupeHandle(HANDLE handle, u64 pid)
	{
		DWORD flags;
		HANDLE re = 0;

		HANDLE cur = GetCurrentProcess();
		HANDLE src = pid ? OpenProcess(GENERIC_ALL, false, pid) : cur;

		if (!DuplicateHandle(src, handle, cur, &re, GENERIC_ALL, 0, DUPLICATE_SAME_ACCESS))
			return 0;


		WIN32_ASSERT(GetHandleInformation(src, &flags));
		WIN32_ASSERT(GetHandleInformation(cur, &flags));
		WIN32_ASSERT(GetHandleInformation(re, &flags));

		WIN32_ASSERT(CloseHandle(src));

		return re;
	}

	u64 PlatformGetCurrentProcessId()
	{
		return GetCurrentProcessId();
	}

	std::string GetLastErrorAsString()
	{
		// Get the error message ID, if any.
		DWORD errorMessageID = GetLastError();
		if (errorMessageID == 0)
		{
			return std::string(); // No error message has been recorded
		}

		LPSTR messageBuffer = nullptr;

		// Ask Win32 to give us the string version of that message ID.
		// The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
		size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			errorMessageID,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&messageBuffer,
			0,
			NULL);

		// Copy the error message into a std::string.
		std::string message(messageBuffer, size);

		// Free the Win32's string's buffer.
		LocalFree(messageBuffer);

		return message;
	}

	void SetThreadName(NOS_HANDLE handle, std::string const& threadName)
	{
		const uint32_t MS_VC_EXCEPTION = 0x406D1388;

		struct THREADNAME_INFO
		{
			uint32_t dwType;    // Must be 0x1000.
			LPCSTR szName;     // Pointer to name (in user addr space).
			uint32_t dwThreadID; // Thread ID (-1=caller thread).
			uint32_t dwFlags;   // Reserved for future use, must be zero.
		};

		THREADNAME_INFO ThreadNameInfo;
		ThreadNameInfo.dwType = 0x1000;
		ThreadNameInfo.szName = threadName.c_str();
		ThreadNameInfo.dwThreadID = ::GetThreadId(reinterpret_cast<HANDLE>(handle));
		ThreadNameInfo.dwFlags = 0;

		__try
		{
			RaiseException(MS_VC_EXCEPTION, 0, sizeof(ThreadNameInfo) / sizeof(ULONG_PTR), (ULONG_PTR*)&ThreadNameInfo);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) __pragma(warning(suppress : 6322))
		{
		}
	}

	NOS_HANDLE GetCurrentThread()
	{
		return ::GetCurrentThread();
	}

	VkExternalMemoryHandleTypeFlagBits GetPlatformMemoryHandleType()
	{
		return VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	}

} //namespace nos::vk
#endif //_WIN32


