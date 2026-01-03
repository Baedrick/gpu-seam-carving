#pragma once

#include "base/base_strings.hpp"
#include "base/base_types.hpp"

namespace dk {
	struct OS_Handle {
		u64 v;

		auto operator==(OS_Handle o) const noexcept -> b8 {
			return v == o.v;
		}
	};
	static_assert(sizeof(OS_Handle::v) >= sizeof(void *));
	
	struct OS_SystemInfo {
		u64 page_size;
		u32 logical_processor_count;
	};

	enum OS_AccessFlags : u8 {
		OS_ACCESS_FLAG_NONE = 0,
		OS_ACCESS_FLAG_READ = 1u << 0,
		OS_ACCESS_FLAG_WRITE = 1u << 1,
		OS_ACCESS_FLAG_APPEND = 1u << 2,
	};

	struct OS_FileAttributes {
		u64 size;
	};

	using OS_ThreadFunction = void (*)(void *params);


	/* --- Handle Type Functions (implemented once) --- */

	auto os_handle_invalid() noexcept -> OS_Handle;


	/* --- System Info (implemented per-os) --- */

	auto os_get_system_info() noexcept -> OS_SystemInfo *;


	/* --- Aborting (implemented per-os) --- */

	[[noreturn]] auto os_abort(s32 exit_code) noexcept -> void;


	/* --- Memory Allocation (implemented per-os) --- */

	auto os_reserve(u64 size) noexcept -> void *;

	auto os_commit(void *ptr, u64 size) noexcept -> b8;

	auto os_decommit(void *ptr, u64 size) noexcept -> void;

	auto os_release(void *ptr, u64 size) noexcept -> void;


	/* --- File System (implemented per-os) --- */

	auto os_file_open(String8 path, OS_AccessFlags flags) noexcept -> OS_Handle;

	auto os_file_close(OS_Handle file) noexcept -> void;

	auto os_attributes_from_file(OS_Handle file) noexcept -> OS_FileAttributes;

	auto os_file_read(OS_Handle file, u64 begin, u64 end, void *out_data) noexcept -> u64;

	auto os_file_write(OS_Handle file, u64 begin, u64 end, void const *data) noexcept -> u64;


	/* --- Time (implemented per-os) --- */

	auto os_now_seconds() noexcept -> f64;

	auto os_now_microseconds() noexcept -> u64;
}
