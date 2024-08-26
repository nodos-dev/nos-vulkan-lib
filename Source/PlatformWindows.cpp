
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
	bool PlatformCloseHandle(HANDLE handle)
	{
		DWORD flags;
		return /*GetHandleInformation(handle, &flags) && */CloseHandle(handle);
	}
	
	HANDLE PlatformDupeHandle(u64 pid, HANDLE handle)
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
} //namespace nos::vk
#endif //_WIN32


