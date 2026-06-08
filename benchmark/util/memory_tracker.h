#pragma once

#include <mutex>

namespace bench_util
{

	// Track total system ram across linux and windows
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <sysinfoapi.h>

	inline size_t get_memory_used_by_current_process()
	{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
			return pmc.PrivateUsage;
		}
		return 0;
	}

#elif defined(__unix__) || defined(__unix)
#include <unistd.h>
#include <sys/sysinfo.h>


	inline size_t get_memory_used_by_current_process()
	{
		long rss = 0L;
		FILE* fp = NULL;
		if ((fp = fopen("/proc/self/statm", "r")) == NULL)
			return (size_t)0L;      /* Can't open? */
		if (fscanf(fp, "%*s%ld", &rss) != 1)
		{
			fclose(fp);
			return (size_t)0L;      /* Can't read? */
		}
		fclose(fp);
		return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);
	}

#elif defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

	inline size_t get_memory_used_by_current_process()
	{
		struct task_basic_info info;
		mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
		kern_return_t ret = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count);
		if (ret != KERN_SUCCESS) {
			return 0;
		}
		return (size_t)info.resident_size;
	}

#else


	inline size_t get_memory_used_by_current_process()
	{
		return 0;
	}

#endif


	struct MemoryAllocTracker
	{
		inline static size_t get_bytes_used() noexcept { return get_memory_used_by_current_process(); }
		inline static double get_kb_used() noexcept { return static_cast<double>(get_memory_used_by_current_process()) / 1024; }
		inline static double get_mb_used() noexcept { return static_cast<double>(get_memory_used_by_current_process()) / 1024 / 1024; }
		inline static double get_gb_used() noexcept { return static_cast<double>(get_memory_used_by_current_process()) / 1024 / 1024 / 1024; }
	};

} // bench_util