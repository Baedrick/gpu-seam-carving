#pragma once

#include "os/os_core.hpp"

namespace dk {
	struct OS_Win32_Context {
		OS_SystemInfo system_info;
		u64 perf_frequency;
	};
	extern OS_Win32_Context os_win32_context;
}
