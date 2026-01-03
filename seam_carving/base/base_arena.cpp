#include "base_arena.hpp"

#include "base/base_assert.h"
#include "base/base_math.hpp"
#include "base/base_strings.hpp"
#include "os/os_core.hpp"
#include "os/os_gfx.hpp"

#include <cstring>

auto dk::arena_alloc(ArenaParams const *params) noexcept -> Arena * {
	DK_ASSERT(params != nullptr);

	u64 const reserve_size = params->reserve_size > 0 ? params->reserve_size : ARENA_DEFAULT_RESERVE_SIZE;
	u64 const commit_size = params->commit_size > 0 ? params->commit_size : ARENA_DEFAULT_COMMIT_SIZE;

	void *const memory = os_reserve(reserve_size);
	if (memory == nullptr) {
		os_show_dialog(
			os_handle_invalid(),
			OS_DialogIcon::ICON_ERROR,
			str8_literal("Fatal Allocation Failure"),
			str8_literal("Unexpected memory allocation failure.")
		);
		os_abort(1);
	}

	u64 const page_size = os_get_system_info()->page_size;
	u64 const initial_commit = align_forward_pow_2(sizeof(Arena), page_size);

	if (!os_commit(memory, initial_commit)) {
		os_release(memory, reserve_size);
		os_show_dialog(
			os_handle_invalid(),
			OS_DialogIcon::ICON_ERROR,
			str8_literal("Fatal Allocation Failure"),
			str8_literal("Unexpected memory allocation failure.")
		);
		os_abort(1);
	}

	Arena *const arena = static_cast<Arena*>(memory);
	arena->memory = memory;
	arena->commit_size = commit_size;
	arena->reserve_size = reserve_size;
	arena->base_offset = align_forward_pow_2(sizeof(Arena), 16);
	arena->offset = arena->base_offset;
	arena->committed = initial_commit;
	arena->reserved = reserve_size;

	return arena;
}

auto dk::arena_release(Arena *arena) noexcept -> void {
	DK_ASSERT(arena != nullptr);

	os_release(arena->memory, arena->reserved);
}

auto dk::arena_push_no_zero(Arena *arena, usize size, usize align) noexcept -> void * {
	DK_ASSERT(arena != nullptr);

	u64 const aligned_offset = align_forward_pow_2(arena->offset, align);
	u64 const new_offset = aligned_offset + size;

	if (new_offset > arena->reserved) {
		return nullptr;
	}

	if (new_offset > arena->committed) {
		u64 const needed = new_offset - arena->committed;
		u64 const size_to_commit = align_forward_pow_2(needed, arena->commit_size);
		os_commit(static_cast<u8 *>(arena->memory) + arena->committed, size_to_commit);
		arena->committed += size_to_commit;
	}

	void *result = static_cast<u8*>(arena->memory) + aligned_offset;
	arena->offset = new_offset;
	return result;
}

auto dk::arena_push(Arena *arena, usize size, usize align) noexcept -> void * {
	void *result = arena_push_no_zero(arena, size, align);
	if (result != nullptr) {
		std::memset(result, 0, size);
	}
	return result;
}

auto dk::arena_push_array(Arena *arena, u64 count, usize size, usize align) noexcept -> void * {
	return arena_push(arena, count * size, align);
}

auto dk::arena_clear(Arena *arena) noexcept -> void {
	arena->offset = arena->base_offset;
}

auto dk::arena_pop(Arena *arena, usize amount) noexcept -> void {
	u64 const old_pos = arena->offset;
	u64 new_pos = 0;
	if (amount < old_pos) {
		new_pos = old_pos - amount;
	}
	arena_pop_to(arena, new_pos);
}

auto dk::arena_pop_to(Arena *arena, u64 position) noexcept -> void {
	position = glm::max(position, arena->base_offset);
	position = glm::min(position, arena->offset);
	arena->offset = position;
}

auto dk::arena_scratch_begin(Arena *arena) noexcept -> ScratchArena {
	return { .arena = arena, .position = arena->offset };
}

auto dk::arena_scratch_end(ScratchArena scratch) noexcept -> void {
	DK_ASSERT(scratch.arena != nullptr);

	scratch.arena->offset = scratch.position;
}
