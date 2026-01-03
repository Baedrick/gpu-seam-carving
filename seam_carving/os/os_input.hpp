#pragma once

#include "base/base_types.hpp"
#include "base/base_math.hpp"
#include "os/os_gfx_input_codes.hpp"

namespace dk {
	struct Arena;
	struct OS_EventList;
	struct OS_InputState;

	auto os_input_create(Arena *arena) noexcept -> OS_InputState *;

	auto os_input_update(OS_InputState *input, OS_EventList const *events) noexcept -> void;


	/* --- Keyboard --- */

	auto os_input_key_held(OS_InputState const *input, OS_Key key) noexcept -> b8;

	auto os_input_key_pressed(OS_InputState const *input, OS_Key key) noexcept -> b8;

	auto os_input_key_released(OS_InputState const *input, OS_Key key) noexcept -> b8;


	/* --- Mouse --- */

	auto os_input_mouse_button_held(OS_InputState const *input, OS_MouseButton button) noexcept -> b8;

	auto os_input_mouse_button_pressed(OS_InputState const *input, OS_MouseButton button) noexcept -> b8;

	auto os_input_mouse_button_released(OS_InputState const *input, OS_MouseButton button) noexcept -> b8;

	auto os_input_mouse_pos(OS_InputState const *input) noexcept -> vec2;

	auto os_input_mouse_delta(OS_InputState const *input) noexcept -> vec2;

	auto os_input_scroll_delta(OS_InputState const *input) noexcept -> vec2;
}