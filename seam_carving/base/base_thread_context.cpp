#include "base_thread_context.hpp"

#include "base/base_assert.h"

namespace {
	thread_local dk::ThreadContext *tc_thread_local = nullptr;
}

auto dk::tc_alloc() noexcept -> ThreadContext * {
	constexpr ArenaParams params = {
		.reserve_size = ARENA_DEFAULT_RESERVE_SIZE,
		.commit_size = ARENA_DEFAULT_COMMIT_SIZE
	};
	Arena *arena = arena_alloc(&params);
	ThreadContext *thread_context = arena_push_type<ThreadContext>(arena);
	thread_context->scratch_arenas[0] = arena;
	thread_context->scratch_arenas[1] = arena_alloc(&params);
	return thread_context;
}

auto dk::tc_release(ThreadContext *context) noexcept -> void {
	DK_ASSERT(context != nullptr);

	arena_release(context->scratch_arenas[1]);
	arena_release(context->scratch_arenas[0]);
}

auto dk::tc_select(ThreadContext *context) noexcept -> void {
	tc_thread_local = context;
}

auto dk::tc_get_selected() noexcept -> ThreadContext * {
	return tc_thread_local;
}

auto dk::tc_get_scratch(Arena **conflicts, u32 count) noexcept -> Arena * {
	ThreadContext *context = tc_get_selected();
	DK_ASSERT(context != nullptr);

	for (Arena *const candidate_arena : context->scratch_arenas) {
		b8 is_conflicting = false;
		for (u32 j = 0; j < count; ++j) {
			if (candidate_arena == conflicts[j]) {
				is_conflicting = true;
				break;
			}
		}
		if (!is_conflicting) {
			return candidate_arena;
		}
	}
	return nullptr;
}
