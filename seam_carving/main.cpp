/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

extern auto entry_point(int argc, char **argv) noexcept -> int;

#ifdef _WIN32
#include "base/base_strings.hpp"
#include "base/base_thread_context.hpp"
#include "os/os_core_win32.hpp"

#include <Windows.h>
#include <cstdio>

namespace {
	auto win32_out_of_memory() noexcept -> int {
		MessageBoxA(nullptr, "Out of memory - aborting", "Fatal Error", MB_ICONERROR | MB_OK);
		return 0;
	}
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR p_cmd_line, int n_cmd_show) {
	UNREFERENCED_PARAMETER(h_instance);
	UNREFERENCED_PARAMETER(h_prev_instance);
	UNREFERENCED_PARAMETER(p_cmd_line);
	UNREFERENCED_PARAMETER(n_cmd_show);

	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		std::FILE *fp = nullptr;
		(void)freopen_s(&fp, "CONOUT$", "w", stdout);
		(void)freopen_s(&fp, "CONOUT$", "w", stderr);
		(void)freopen_s(&fp, "CONIN$", "r", stdin);
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
		dk::os_abort(1);
	}

	SYSTEM_INFO sys_info = {};
	GetSystemInfo(&sys_info);

	{
		dk::OS_SystemInfo *info = &dk::os_win32_context.system_info;
		info->logical_processor_count = static_cast<dk::u32>(sys_info.dwNumberOfProcessors);
		info->page_size = sys_info.dwPageSize;
	}
	{
		dk::os_win32_context.perf_frequency = 1;
		LARGE_INTEGER freq{};
		if (QueryPerformanceFrequency(&freq)) {
			dk::os_win32_context.perf_frequency = freq.QuadPart;
		}
	}

	dk::ThreadContext *thread_context = dk::tc_alloc();
	dk::tc_select(thread_context);

	int argc = 0;
	LPWSTR *argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argvw == nullptr) {
		return win32_out_of_memory();
	}

	constexpr dk::ArenaParams args_arena_params = {
		.reserve_size = dk::mega_bytes(1),
		.commit_size = dk::kilo_bytes(32)
	};
	dk::Arena *args_arena = dk::arena_alloc(&args_arena_params);

	char **argv = dk::arena_push_type_array<char *>(args_arena, argc + 1);
	for (int i = 0; i < argc; ++i) {
		dk::u16 const *arg16_ptr = reinterpret_cast<dk::u16 const *>(argvw[i]);
		dk::u64 arg16_len = 0;
		while (arg16_ptr[arg16_len] != L'\0') {
			arg16_len += 1;
		}

		dk::String16 const arg16 = { .data = arg16_ptr, .size = arg16_len };
		dk::String8 const arg8 = dk::str8_from_16(args_arena, arg16);
		argv[i] = const_cast<char *>(reinterpret_cast<char const *>(arg8.data));
	}
	argv[argc] = nullptr;
	LocalFree(static_cast<void *>(argvw));

	int const result = entry_point(argc, argv);

	dk::arena_release(args_arena);

	dk::tc_select(nullptr);
	dk::tc_release(thread_context);

	CoUninitialize();

	return result;
}
#else
#	error "Not implemented for this platform"
#endif
