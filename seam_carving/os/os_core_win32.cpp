#include "os_core.hpp"
#include "os_core_win32.hpp"

#include "base/base_math.hpp"
#include "base/base_thread_context.hpp"

#define NOMINMAX
#include <Windows.h>

namespace dk {
	OS_Win32_Context os_win32_context;
}

auto dk::os_get_system_info() noexcept -> OS_SystemInfo * {
	return &os_win32_context.system_info;
}

auto dk::os_abort(s32 exit_code) noexcept -> void {
	ExitProcess(exit_code);
}

auto dk::os_reserve(u64 size) noexcept -> void * {
	return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
}

auto dk::os_commit(void *ptr, u64 size) noexcept -> b8 {
	return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

auto dk::os_decommit(void *ptr, u64 size) noexcept -> void {
	VirtualFree(ptr, size, MEM_DECOMMIT);
}

auto dk::os_release(void *ptr, u64 size) noexcept -> void {
	// NOTE(Dedrick): Size not used, not necessary on Windows.
	(void)size;
	VirtualFree(ptr, 0, MEM_RELEASE);
}

auto dk::os_file_open(String8 path, OS_AccessFlags flags) noexcept -> OS_Handle {
	ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(nullptr, 0));
	String16 const path16 = str16_from_8(scratch.arena, path);

	DWORD access_flags = 0;
	DWORD creation_disposition = OPEN_EXISTING;
	SECURITY_ATTRIBUTES security_attributes = { sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE };

	if (flags & OS_ACCESS_FLAG_READ) { access_flags |= GENERIC_READ; }
	if (flags & OS_ACCESS_FLAG_WRITE) { access_flags |= GENERIC_WRITE; }

	if (flags & OS_ACCESS_FLAG_WRITE) { creation_disposition = CREATE_ALWAYS; }
	if (flags & OS_ACCESS_FLAG_APPEND) { creation_disposition = OPEN_ALWAYS; access_flags |= FILE_APPEND_DATA; }

	OS_Handle result = os_handle_invalid();
	HANDLE const file = CreateFileW(
		reinterpret_cast<WCHAR const *>(path16.data),
		access_flags,
		0,
		&security_attributes,
		creation_disposition,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (file != INVALID_HANDLE_VALUE) {
		result.v = reinterpret_cast<uintptr_t>(file);
	}
	arena_scratch_end(scratch);
	return result;
}

auto dk::os_file_close(OS_Handle file) noexcept -> void {
	if (file == os_handle_invalid()) {
		return;
	}
	HANDLE const handle = reinterpret_cast<HANDLE>(file.v);
	BOOL const result = CloseHandle(handle);
	(void)result;
}

auto dk::os_attributes_from_file(OS_Handle file) noexcept -> OS_FileAttributes {
	DK_ASSERT(file != os_handle_invalid());

	HANDLE const handle = reinterpret_cast<HANDLE>(file.v);
	u32 high_bits = 0;
	u32 const low_bits = GetFileSize(handle, reinterpret_cast<DWORD *>(&high_bits));
	OS_FileAttributes attributes = {};
	attributes.size = static_cast<u64>(low_bits) | static_cast<u64>(high_bits) << 32;
	return attributes;
}

auto dk::os_file_read(OS_Handle file, u64 begin, u64 end, void *out_data) noexcept -> u64 {
	if (file == os_handle_invalid()) {
		return 0;
	}

	HANDLE const handle = reinterpret_cast<HANDLE>(file.v);
	
	// NOTE(Dedrick): Clamp range by file size.
	u64 size = 0;
	GetFileSizeEx(handle, reinterpret_cast<LARGE_INTEGER *>(&size));

	u64 const clamped_begin = glm::clamp<u64>(begin, 0, size);
	u64 const clamped_end = glm::clamp<u64>(end, 0, size);
	u64 const to_read = clamped_end > clamped_begin ? clamped_end - clamped_begin : 0;

	u64 total_read_size = 0;
	u64 current_offset = clamped_begin;

	while (total_read_size < to_read) {
		u64 const remaining = to_read - total_read_size;
		DWORD const read_amt = static_cast<DWORD>(glm::min<u64>(remaining, 0xFFFFFFFF));
		
		// NOTE(Dedrick): We use an overlapped structure because it
		// avoids the call to SetFilePointer (syscall) and is multithreading safe.
		OVERLAPPED overlapped = {};
		overlapped.Offset = static_cast<DWORD>(current_offset & 0xFFFFFFFF);
		overlapped.OffsetHigh = static_cast<DWORD>((current_offset >> 32) & 0xFFFFFFFF);

		DWORD bytes_read = 0;
		if (!ReadFile(
				handle,
				static_cast<u8 *>(out_data) + total_read_size,
				read_amt,
				&bytes_read,
				&overlapped)) {
			break; 
		}

		if (bytes_read == 0) {
			break;
		}

		total_read_size += bytes_read;
		current_offset += bytes_read;
		
		if (bytes_read != read_amt) {
			break;
		}
	}

	return total_read_size;
}

auto dk::os_file_write(OS_Handle file, u64 begin, u64 end, void const *data) noexcept -> u64 {
	if (file == os_handle_invalid()) {
		return 0;
	}

	HANDLE const handle = reinterpret_cast<HANDLE>(file.v);

	u64 const to_write = (end > begin) ? (end - begin) : 0;

	u64 total_written_size = 0;
	u64 current_offset = begin;

	while (total_written_size < to_write) {
		u64 const remaining = to_write - total_written_size;
		constexpr u64 chunk_size = mega_bytes(1);
		DWORD const write_amt = static_cast<DWORD>(glm::min(remaining, chunk_size));

		// NOTE(Dedrick): We use an overlapped structure because it
		// avoids the call to SetFilePointer (syscall) and is multithreading safe.
		OVERLAPPED overlapped = {};
		overlapped.Offset = static_cast<DWORD>(current_offset & 0xFFFFFFFF);
		overlapped.OffsetHigh = static_cast<DWORD>((current_offset >> 32) & 0xFFFFFFFF);

		DWORD bytes_written = 0;
		if (!WriteFile(
				handle,
				static_cast<u8 const *>(data) + total_written_size,
				write_amt,
				&bytes_written,
				&overlapped)) {
			break;
		}

		total_written_size += bytes_written;
		current_offset += bytes_written;
	}

	return total_written_size;
}

auto dk::os_now_seconds() noexcept -> f64 {
	LARGE_INTEGER current_time = {};
	QueryPerformanceCounter(&current_time);
	f64 const time_s = static_cast<f64>(current_time.QuadPart) / static_cast<f64>(os_win32_context.perf_frequency);
	return time_s;
}

auto dk::os_now_microseconds() noexcept -> u64 {
	LARGE_INTEGER current_time = {};
	QueryPerformanceCounter(&current_time);
	u64 const time_us = (current_time.QuadPart * 1000000) / os_win32_context.perf_frequency;
	return time_us;
}
