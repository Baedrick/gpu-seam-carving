#include "os_input.hpp"

#include "base/base_arena.hpp"
#include "os/os_gfx.hpp"
#include "os/os_gfx_input_codes.hpp"

#include <cstring>

namespace dk {
	struct OS_InputState {
		u64 keys_curr[4];
		u64 keys_prev[4];

		u64 mouse_curr;
		u64 mouse_prev;

		vec2 mouse_pos;
		vec2 mouse_delta;
		vec2 scroll_delta;
	};
}

namespace {
	auto os_input_set_bit(dk::u64 *bits, dk::u8 index) noexcept -> void {
		bits[index / 64] |= (1ull << (index % 64));
	}

	auto os_input_clear_bit(dk::u64 *bits, dk::u8 index) noexcept -> void {
		bits[index / 64] &= ~(1ull << (index % 64));
	}

	auto os_input_check_bit(dk::u64 const *bits, dk::u8 index) noexcept -> dk::b8 {
		return (bits[index / 64] & (1ull << (index % 64))) != 0;
	}
}

auto dk::os_input_create(Arena *arena) noexcept -> OS_InputState * {
	OS_InputState *state = arena_push_type<OS_InputState>(arena);
	return state;
}

auto dk::os_input_update(OS_InputState *input, OS_EventList const *events) noexcept -> void {
	DK_ASSERT(input != nullptr);

	std::memcpy(input->keys_prev, input->keys_curr, sizeof(input->keys_curr));
	input->mouse_prev = input->mouse_curr;
	
	input->mouse_delta = vec2(0.0f);
	input->scroll_delta = vec2(0.0f);

	for (OS_Event const *event = events->first; event != nullptr; event = event->next) {
		switch (event->type) {
			case OS_EventType::KEY_DOWN: {
				os_input_set_bit(input->keys_curr, static_cast<u8>(event->data.key.key));
				break;
			}
			case OS_EventType::KEY_UP: {
				os_input_clear_bit(input->keys_curr, static_cast<u8>(event->data.key.key));
				break;
			}
			case OS_EventType::MOUSE_BUTTON_DOWN: {
				input->mouse_curr |= (1ull << static_cast<u8>(event->data.button.button));
				break;
			}
			case OS_EventType::MOUSE_BUTTON_UP: {
				input->mouse_curr &= ~(1ull << static_cast<u8>(event->data.button.button));
				break;
			}
			case OS_EventType::MOUSE_MOTION: {
				input->mouse_delta.x += (event->data.mouse_move.x - input->mouse_pos.x);
				input->mouse_delta.y += (event->data.mouse_move.y - input->mouse_pos.y);
				input->mouse_pos.x = event->data.mouse_move.x;
				input->mouse_pos.y = event->data.mouse_move.y;
				break;
			}
			case OS_EventType::MOUSE_WHEEL: {
				input->scroll_delta.x += event->data.mouse_wheel.dx;
				input->scroll_delta.y += event->data.mouse_wheel.dy;
				break;
			}
			case OS_EventType::WINDOW_FOCUS_LOST: {
				std::memset(input->keys_curr, 0, sizeof(input->keys_curr));
				input->mouse_curr = 0;
				break;
			}
			default: break;
		}
	}
}

auto dk::os_input_key_held(OS_InputState const *input, OS_Key key) noexcept -> b8 {
	DK_ASSERT(input != nullptr);

	return os_input_check_bit(input->keys_curr, static_cast<u8>(key));
}

auto dk::os_input_key_pressed(OS_InputState const *input, OS_Key key) noexcept -> b8 {
	DK_ASSERT(input != nullptr);

	return os_input_check_bit(input->keys_curr, static_cast<u8>(key))
		&& !os_input_check_bit(input->keys_prev, static_cast<u8>(key));
}

auto dk::os_input_key_released(OS_InputState const *input, OS_Key key) noexcept -> b8 {
	DK_ASSERT(input != nullptr);

	return !os_input_check_bit(input->keys_curr, static_cast<u8>(key))
		&& os_input_check_bit(input->keys_prev, static_cast<u8>(key));
}

auto dk::os_input_mouse_button_held(OS_InputState const *input, OS_MouseButton button) noexcept -> b8 {
	DK_ASSERT(input != nullptr);

	return (input->mouse_curr & (1ull << static_cast<u8>(button))) != 0;
}

auto dk::os_input_mouse_button_pressed(OS_InputState const *input, OS_MouseButton button) noexcept -> b8 {
	DK_ASSERT(input != nullptr);

	u64 const mask = 1ull << static_cast<u8>(button);
	return (input->mouse_curr & mask) && !(input->mouse_prev & mask);
}

auto dk::os_input_mouse_button_released(OS_InputState const *input, OS_MouseButton button) noexcept -> b8 {
	DK_ASSERT(input != nullptr);

	u64 const mask = 1ull << static_cast<u8>(button);
	return !(input->mouse_curr & mask) && (input->mouse_prev & mask);
}

auto dk::os_input_mouse_pos(OS_InputState const *input) noexcept -> vec2 {
	DK_ASSERT(input != nullptr);

	return input->mouse_pos;
}

auto dk::os_input_mouse_delta(OS_InputState const *input) noexcept -> vec2 {
	DK_ASSERT(input != nullptr);

	return input->mouse_delta;
}

auto dk::os_input_scroll_delta(OS_InputState const *input) noexcept -> vec2 {
	DK_ASSERT(input != nullptr);

	return input->scroll_delta;
}
