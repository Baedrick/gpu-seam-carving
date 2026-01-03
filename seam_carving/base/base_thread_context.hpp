#pragma once

#include "base/base_arena.hpp"
#include "base/base_types.hpp"

namespace dk {
	struct ThreadContext {
		Arena *scratch_arenas[2];
	};

	auto tc_alloc() noexcept -> ThreadContext *;

	auto tc_release(ThreadContext *context) noexcept -> void;

	auto tc_select(ThreadContext *context) noexcept -> void;

	auto tc_get_selected() noexcept -> ThreadContext *;

	auto tc_get_scratch(Arena **conflicts, u32 count) noexcept -> Arena *;
}
