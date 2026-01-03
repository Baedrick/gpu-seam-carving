#pragma once

#include "base/base_types.hpp"
#include "base/base_utils.hpp"

namespace dk {
	struct ArenaParams {
		u64 reserve_size;
		u64 commit_size;
	};

	struct Arena {
		void *memory; ///< Memory owned by the arena.
		u64 commit_size;
		u64 reserve_size;
		u64 base_offset;
		u64 offset;
		u64 committed;
		u64 reserved;
	};

	struct ScratchArena {
		Arena *arena;
		u64 position;
	};

	constexpr u64 ARENA_DEFAULT_RESERVE_SIZE = mega_bytes(64);
	constexpr u64 ARENA_DEFAULT_COMMIT_SIZE = kilo_bytes(64);

	auto arena_alloc(ArenaParams const *params) noexcept -> Arena *;

	auto arena_release(Arena *arena) noexcept -> void;

	auto arena_push(Arena *arena, usize size, usize align) noexcept -> void *;
	
	auto arena_push_no_zero(Arena *arena, usize size, usize align) noexcept -> void *;

	auto arena_push_array(Arena *arena, u64 count, usize size, usize align) noexcept -> void *;

	auto arena_clear(Arena *arena) noexcept -> void;

	auto arena_pop(Arena *arena, usize amount) noexcept -> void;

	auto arena_pop_to(Arena *arena, u64 position) noexcept -> void;

	auto arena_scratch_begin(Arena *arena) noexcept -> ScratchArena;

	auto arena_scratch_end(ScratchArena scratch) noexcept -> void;

	template <typename T>
	auto arena_push_type(Arena *arena) noexcept -> T *{
		return static_cast<T *>(arena_push(arena, sizeof(T), alignof(T)));
	}

	template <typename T>
	auto arena_push_type_array(Arena *arena, u64 count) noexcept -> T * {
		return static_cast<T *>(arena_push_array(arena, count, sizeof(T), alignof(T)));
	}
}
