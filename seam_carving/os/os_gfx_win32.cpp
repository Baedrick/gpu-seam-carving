#include "os_gfx.hpp"
#include "os_gfx_win32.hpp"

#include "base/base_assert.h"
#include "base/base_containers.hpp"
#include "base/base_thread_context.hpp"
#include "os/os_gfx_input_codes.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <Windows.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <ShlObj_core.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace dk {
	OS_Win32_GfxContext *os_win32_gfx_context;
}

auto dk::os_win32_handle_from_window(OS_Win32_Window *window) noexcept -> OS_Handle {
	return { reinterpret_cast<u64>(window) };
}

auto dk::os_win32_window_from_handle(OS_Handle handle) noexcept -> OS_Win32_Window * {
	return reinterpret_cast<OS_Win32_Window *>(handle.v);
}

namespace {
	auto os_win32_push_event(dk::OS_EventType type, dk::OS_Handle window_handle) noexcept -> dk::OS_Event * {
		using namespace dk;
		
		Arena *target_arena = os_win32_gfx_context->active_events_arena;
		OS_EventList *target_list = os_win32_gfx_context->active_events_list;

		OS_Event *event = arena_push_type<OS_Event>(target_arena);
		*event = {};
		event->type = type;
		event->data.window_common.window = window_handle;

		list_push_back(&target_list->first, &target_list->last, event);
		++target_list->count;

		return event;
	}

	auto glfw_window_close_callback(GLFWwindow *window) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		os_win32_push_event(OS_EventType::WINDOW_CLOSE, handle);
	}

	auto glfw_window_size_callback(GLFWwindow *window, dk::s32 width, dk::s32 height) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		OS_Event *event = os_win32_push_event(OS_EventType::WINDOW_RESIZE, handle);
		event->data.window_resize.width = width;
		event->data.window_resize.height = height;
	}

	auto glfw_window_content_scale_callback(GLFWwindow *window, dk::f32 x_scale, dk::f32 y_scale) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		OS_Event *event = os_win32_push_event(OS_EventType::WINDOW_CONTENT_SCALE_CHANGED, handle);
		event->data.window_content_scale.x_scale = x_scale;
		event->data.window_content_scale.y_scale = y_scale;
	}

	auto glfw_window_minimize_callback(GLFWwindow *window, dk::s32 iconified) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		if (iconified) {
			os_win32_push_event(OS_EventType::WINDOW_MINIMIZED, handle);
		} else {
			os_win32_push_event(OS_EventType::WINDOW_RESTORED, handle); // Restored from minimize
		}
	}

	auto glfw_window_maximize_callback(GLFWwindow *window, dk::s32 maximized) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		if (maximized) {
			os_win32_push_event(OS_EventType::WINDOW_MAXIMIZED, handle);
		} else {
			os_win32_push_event(OS_EventType::WINDOW_RESTORED, handle); // Restored from maximize
		}
	}

	auto glfw_window_focus_callback(GLFWwindow *window, dk::s32 focused) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		if (focused) {
			os_win32_push_event(OS_EventType::WINDOW_FOCUS_GAINED, handle);
		} else {
			os_win32_push_event(OS_EventType::WINDOW_FOCUS_LOST, handle);
		}
	}

	auto glfw_key_callback(GLFWwindow *window, dk::s32 key, dk::s32 /*scancode*/, dk::s32 action, dk::s32 /*mods*/) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		OS_Key const os_key = os_win32_gfx_context->key_from_glfw_key_table[key];
		if (os_key == OS_Key::KEY_NONE) {
			return;
		}

		if (action == GLFW_PRESS || action == GLFW_REPEAT) {
			OS_Event *event = os_win32_push_event(OS_EventType::KEY_DOWN, handle);
			event->data.key.key = os_key;
		} else if (action == GLFW_RELEASE) {
			OS_Event *event = os_win32_push_event(OS_EventType::KEY_UP, handle);
			event->data.key.key = os_key;
		}
	}

	auto glfw_mouse_button_callback(GLFWwindow *window, dk::s32 button, dk::s32 action, dk::s32 /*mods*/) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		OS_MouseButton const os_button = os_win32_gfx_context->button_from_glfw_button_table[button];
		if (os_button == OS_MouseButton::BUTTON_NONE) {
			return;
		}

		if (action == GLFW_PRESS) {
			OS_Event *event = os_win32_push_event(OS_EventType::MOUSE_BUTTON_DOWN, handle);
			event->data.button.button = os_button;
		} else if (action == GLFW_RELEASE) {
			OS_Event *event = os_win32_push_event(OS_EventType::MOUSE_BUTTON_UP, handle);
			event->data.button.button = os_button;
		}
	}

	auto glfw_cursor_pos_callback(GLFWwindow *window, dk::f64 x_pos, dk::f64 y_pos) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		OS_Event *event = os_win32_push_event(OS_EventType::MOUSE_MOTION, handle);
		event->data.mouse_move.x = static_cast<f32>(x_pos);
		event->data.mouse_move.y = static_cast<f32>(y_pos);
	}

	auto glfw_scroll_callback(GLFWwindow *window, dk::f64 x_offset, dk::f64 y_offset) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle const handle = os_win32_handle_from_window(win32_window);
		OS_Event *event = os_win32_push_event(OS_EventType::MOUSE_WHEEL, handle);
		event->data.mouse_wheel.dx = static_cast<f32>(x_offset);
		event->data.mouse_wheel.dy = static_cast<f32>(y_offset);
	}

	auto glfw_cursor_enter_callback(GLFWwindow *window, dk::s32 entered) noexcept -> void {
		using namespace dk;

		OS_Win32_Window *win32_window = static_cast<OS_Win32_Window *>(glfwGetWindowUserPointer(window));
		OS_Handle handle = os_win32_handle_from_window(win32_window);
		if (entered) {
			os_win32_push_event(OS_EventType::WINDOW_MOUSE_ENTER, handle);
		} else {
			os_win32_push_event(OS_EventType::WINDOW_MOUSE_LEAVE, handle);
		}
	}
}

auto dk::os_gfx_init() noexcept -> void {
	DK_ASSERT(os_win32_gfx_context == nullptr);

	constexpr ArenaParams params = {
		.reserve_size = ARENA_DEFAULT_RESERVE_SIZE,
		.commit_size = ARENA_DEFAULT_COMMIT_SIZE
	};
	Arena *const arena = arena_alloc(&params);
	
	os_win32_gfx_context = arena_push_type<OS_Win32_GfxContext>(arena);
	os_win32_gfx_context->arena = arena;
	os_win32_gfx_context->pending_events_arena = arena_alloc(&params);
	os_win32_gfx_context->pending_events_list = {};
	
	os_win32_gfx_context->active_events_arena = os_win32_gfx_context->pending_events_arena;
	os_win32_gfx_context->active_events_list = &os_win32_gfx_context->pending_events_list;

	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_LEFT_SUPER] = OS_Key::KEY_LSUPER;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_RIGHT_SUPER] = OS_Key::KEY_RSUPER;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_BACKSPACE] = OS_Key::KEY_BACKSPACE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_TAB] = OS_Key::KEY_TAB;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_ENTER] = OS_Key::KEY_ENTER;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_LEFT_SHIFT] = OS_Key::KEY_LSHIFT;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_RIGHT_SHIFT] = OS_Key::KEY_RSHIFT;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_LEFT_CONTROL] = OS_Key::KEY_LCTRL;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_RIGHT_CONTROL] = OS_Key::KEY_RCTRL;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_LEFT_ALT] = OS_Key::KEY_LALT;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_RIGHT_ALT] = OS_Key::KEY_RALT;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_PRINT_SCREEN] = OS_Key::KEY_PRINT_SCREEN;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_SCROLL_LOCK] = OS_Key::KEY_SCROLL_LOCK;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_PAUSE] = OS_Key::KEY_PAUSE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_CAPS_LOCK] = OS_Key::KEY_CAPS_LOCK;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_ESCAPE] = OS_Key::KEY_ESCAPE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_SPACE] = OS_Key::KEY_SPACE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_PAGE_UP] = OS_Key::KEY_PAGE_UP;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_PAGE_DOWN] = OS_Key::KEY_PAGE_DOWN;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_END] = OS_Key::KEY_END;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_HOME] = OS_Key::KEY_HOME;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_LEFT] = OS_Key::KEY_LEFT;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_UP] = OS_Key::KEY_UP;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_RIGHT] = OS_Key::KEY_RIGHT;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_DOWN] = OS_Key::KEY_DOWN;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_INSERT] = OS_Key::KEY_INSERT;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_DELETE] = OS_Key::KEY_DELETE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_0] = OS_Key::KEY_0;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_1] = OS_Key::KEY_1;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_2] = OS_Key::KEY_2;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_3] = OS_Key::KEY_3;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_4] = OS_Key::KEY_4;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_5] = OS_Key::KEY_5;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_6] = OS_Key::KEY_6;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_7] = OS_Key::KEY_7;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_8] = OS_Key::KEY_8;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_9] = OS_Key::KEY_9;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_A] = OS_Key::KEY_A;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_B] = OS_Key::KEY_B;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_C] = OS_Key::KEY_C;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_D] = OS_Key::KEY_D;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_E] = OS_Key::KEY_E;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F] = OS_Key::KEY_F;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_G] = OS_Key::KEY_G;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_H] = OS_Key::KEY_H;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_I] = OS_Key::KEY_I;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_J] = OS_Key::KEY_J;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_K] = OS_Key::KEY_K;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_L] = OS_Key::KEY_L;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_M] = OS_Key::KEY_M;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_N] = OS_Key::KEY_N;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_O] = OS_Key::KEY_O;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_P] = OS_Key::KEY_P;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_Q] = OS_Key::KEY_Q;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_R] = OS_Key::KEY_R;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_S] = OS_Key::KEY_S;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_T] = OS_Key::KEY_T;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_U] = OS_Key::KEY_U;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_V] = OS_Key::KEY_V;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_W] = OS_Key::KEY_W;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_X] = OS_Key::KEY_X;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_Y] = OS_Key::KEY_Y;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_Z] = OS_Key::KEY_Z;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_0] = OS_Key::KEY_NUMPAD_0;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_1] = OS_Key::KEY_NUMPAD_1;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_2] = OS_Key::KEY_NUMPAD_2;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_3] = OS_Key::KEY_NUMPAD_3;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_4] = OS_Key::KEY_NUMPAD_4;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_5] = OS_Key::KEY_NUMPAD_5;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_6] = OS_Key::KEY_NUMPAD_6;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_7] = OS_Key::KEY_NUMPAD_7;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_8] = OS_Key::KEY_NUMPAD_8;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_9] = OS_Key::KEY_NUMPAD_9;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_MULTIPLY] = OS_Key::KEY_NUM_MULTIPLY;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_ADD] = OS_Key::KEY_NUM_PLUS;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_SUBTRACT] = OS_Key::KEY_NUM_MINUS;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_DECIMAL] = OS_Key::KEY_NUM_PERIOD;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_DIVIDE] = OS_Key::KEY_NUM_DIVIDE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_NUM_LOCK] = OS_Key::KEY_NUM_LOCK;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_ENTER] = OS_Key::KEY_NUM_ENTER;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_KP_EQUAL] = OS_Key::KEY_NUM_EQUAL;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F1] = OS_Key::KEY_F1;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F2] = OS_Key::KEY_F2;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F3] = OS_Key::KEY_F3;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F4] = OS_Key::KEY_F4;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F5] = OS_Key::KEY_F5;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F6] = OS_Key::KEY_F6;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F7] = OS_Key::KEY_F7;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F8] = OS_Key::KEY_F8;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F9] = OS_Key::KEY_F9;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F10] = OS_Key::KEY_F10;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F11] = OS_Key::KEY_F11;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_F12] = OS_Key::KEY_F12;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_SEMICOLON] = OS_Key::KEY_SEMICOLON;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_SLASH] = OS_Key::KEY_SLASH;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_GRAVE_ACCENT] = OS_Key::KEY_BACKQUOTE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_LEFT_BRACKET] = OS_Key::KEY_LBRACKET;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_BACKSLASH] = OS_Key::KEY_BACKSLASH;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_RIGHT_BRACKET] = OS_Key::KEY_RBRACKET;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_APOSTROPHE] = OS_Key::KEY_QUOTE;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_EQUAL] = OS_Key::KEY_EQUAL;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_MINUS] = OS_Key::KEY_MINUS;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_PERIOD] = OS_Key::KEY_PERIOD;
	os_win32_gfx_context->key_from_glfw_key_table[GLFW_KEY_COMMA] = OS_Key::KEY_COMMA;

	os_win32_gfx_context->button_from_glfw_button_table[GLFW_MOUSE_BUTTON_1] = OS_MouseButton::BUTTON_1;
	os_win32_gfx_context->button_from_glfw_button_table[GLFW_MOUSE_BUTTON_2] = OS_MouseButton::BUTTON_2;
	os_win32_gfx_context->button_from_glfw_button_table[GLFW_MOUSE_BUTTON_3] = OS_MouseButton::BUTTON_3;
	os_win32_gfx_context->button_from_glfw_button_table[GLFW_MOUSE_BUTTON_4] = OS_MouseButton::BUTTON_4;
	os_win32_gfx_context->button_from_glfw_button_table[GLFW_MOUSE_BUTTON_5] = OS_MouseButton::BUTTON_5;

	if (!glfwInit()) {
		os_show_dialog(
			os_handle_invalid(),
			OS_DialogIcon::ICON_ERROR,
			str8_literal("Fatal Error"),
			str8_literal("Failed to initialize GLFW.")
		);
		os_abort(1);
	}
}

auto dk::os_gfx_shutdown() noexcept -> void {
	DK_ASSERT(os_win32_gfx_context != nullptr);

	glfwTerminate();
	arena_release(os_win32_gfx_context->arena);
	os_win32_gfx_context = nullptr;
}

auto dk::os_get_events(Arena *arena) noexcept -> OS_EventList {
	OS_EventList event_list = {};
	
	// NOTE(Dedrick): Flush pending events.
	if (os_win32_gfx_context->pending_events_list.count > 0) {
		OS_Event const *first = os_win32_gfx_context->pending_events_list.first;
		
		for (OS_Event const *event = first; event != nullptr; event = event->next) {
			OS_Event *new_event = arena_push_type<OS_Event>(arena);
			*new_event = *event;
			
			list_push_back(&event_list.first, &event_list.last, new_event);
			event_list.count++;
		}
		
		arena_clear(os_win32_gfx_context->pending_events_arena);
		os_win32_gfx_context->pending_events_list = {};
	}

	os_win32_gfx_context->active_events_arena = arena;
	os_win32_gfx_context->active_events_list = &event_list;

	glfwPollEvents();

	os_win32_gfx_context->active_events_arena = os_win32_gfx_context->pending_events_arena;
	os_win32_gfx_context->active_events_list = &os_win32_gfx_context->pending_events_list;

	return event_list;
}

auto dk::os_consume_event(OS_EventList *events, OS_Event *event) noexcept -> void {
	DK_ASSERT(events != nullptr);
	DK_ASSERT(event != nullptr);
	DK_ASSERT(events->count > 0);

	list_remove(&events->first, &events->last, event);
	--events->count;
}

auto dk::os_window_open(String8 title, s32 x, s32 y, s32 w, s32 h, OS_WindowFlags flags) noexcept -> OS_Handle {
	title = (title.data == nullptr || *title.data == '\0') ? str8_literal(" ") : title;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, (flags & OS_WINDOW_FLAG_NO_RESIZE) != 0 ? GLFW_FALSE : GLFW_TRUE);

	GLFWwindow *window = glfwCreateWindow(w, h, reinterpret_cast<const char *>(title.data), nullptr, nullptr);
	if (window == nullptr) {
		os_show_dialog(
			os_handle_invalid(),
			OS_DialogIcon::ICON_ERROR,
			str8_literal("Fatal Error"),
			str8_literal("Failed to create GLFW window.")
		);
		return os_handle_invalid();
	}

	if ((flags & OS_WINDOW_FLAG_CENTER) != 0) {
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		GLFWvidmode const *mode = glfwGetVideoMode(monitor);
		s32 const screen_width = mode->width;
		s32 const screen_height = mode->height;
		x = (screen_width - w) / 2;
		y = (screen_height - h) / 2;
	}
	
	glfwSetWindowPos(window, x, y);
	glfwShowWindow(window);
	glfwMakeContextCurrent(window);

	OS_Win32_Window *win32_window = arena_push_type<OS_Win32_Window>(os_win32_gfx_context->arena);
	win32_window->glfw_window = window;

	os_win32_gfx_context->window = win32_window;
	glfwSetWindowUserPointer(window, win32_window);

	glfwSetWindowCloseCallback(window, glfw_window_close_callback);
	glfwSetWindowSizeCallback(window, glfw_window_size_callback);
	glfwSetWindowContentScaleCallback(window, glfw_window_content_scale_callback);
	glfwSetWindowIconifyCallback(window, glfw_window_minimize_callback);
	glfwSetWindowMaximizeCallback(window, glfw_window_maximize_callback);
	glfwSetWindowFocusCallback(window, glfw_window_focus_callback);
	glfwSetKeyCallback(window, glfw_key_callback);
	glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
	glfwSetCursorPosCallback(window, glfw_cursor_pos_callback);
	glfwSetScrollCallback(window, glfw_scroll_callback);
	glfwSetCursorEnterCallback(window, glfw_cursor_enter_callback);

	return os_win32_handle_from_window(win32_window);
}

auto dk::os_window_close(OS_Handle window) noexcept -> void {
	DK_ASSERT(os_win32_gfx_context != nullptr);
	
	if (window == os_handle_invalid()) {
		return;
	}

	OS_Win32_Window const *win32_window = os_win32_window_from_handle(window);
	if (win32_window != nullptr && win32_window->glfw_window != nullptr) {
		glfwDestroyWindow(win32_window->glfw_window);
		if (os_win32_gfx_context->window == win32_window) {
			os_win32_gfx_context->window = nullptr;
		}
	}
}

auto dk::os_window_client_size(OS_Handle window) noexcept -> vec2 {
	DK_ASSERT(window != os_handle_invalid());

	OS_Win32_Window const *win32_window = os_win32_window_from_handle(window);
	s32 width = 0;
	s32 height = 0;
	glfwGetWindowSize(win32_window->glfw_window, &width, &height);
	return { static_cast<f32>(width), static_cast<f32>(height) };
}

auto dk::os_window_content_scale(OS_Handle window) noexcept -> f32 {
	DK_ASSERT(window != os_handle_invalid());

	OS_Win32_Window const *win32_window = os_win32_window_from_handle(window);
	f32 x_scale = 0.0f;
	f32 y_scale = 0.0f;
	glfwGetWindowContentScale(win32_window->glfw_window, &x_scale, &y_scale);
	return x_scale < y_scale ? x_scale : y_scale;
}

auto dk::os_window_swap_interval(s32 interval) noexcept -> void {
	glfwSwapInterval(interval);
}

auto dk::os_window_present(OS_Handle window) noexcept -> void {
	OS_Win32_Window const *win32_window = os_win32_window_from_handle(window);
	glfwSwapBuffers(win32_window->glfw_window);
}

auto dk::os_show_dialog(OS_Handle parent, OS_DialogIcon icon, String8 title, String8 message) noexcept -> void {
	UINT style = MB_OK;
	if (icon == OS_DialogIcon::ICON_INFO) { style |= MB_ICONINFORMATION; }
	if (icon == OS_DialogIcon::ICON_WARNING) { style |= MB_ICONWARNING; }
	if (icon == OS_DialogIcon::ICON_ERROR) { style |= MB_ICONERROR; }
	ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(nullptr, 0));
	String16 const title16 = str16_from_8(scratch.arena, title);
	String16 const message16 = str16_from_8(scratch.arena, message);
	
	OS_Win32_Window const *parent_win = os_win32_window_from_handle(parent);
	HWND const hwnd = (parent_win != nullptr) ? glfwGetWin32Window(parent_win->glfw_window) : nullptr;

	MessageBoxW(
		hwnd,
		reinterpret_cast<WCHAR const *>(message16.data),
		reinterpret_cast<WCHAR const *>(title16.data),
		MB_OK | style
	);
	arena_scratch_end(scratch);
}

auto dk::os_show_in_file_browser(String8 path) noexcept -> void {
	ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(nullptr, 0));
	String8 const path_copy = str8_copy(scratch.arena, path);

	// NOTE(Dedrick): Shell functions inconsistently expect backwards slash.
	u8 *path_str = const_cast<u8 *>(path_copy.data);
	for (u64 i = 0; i < path_copy.size; ++i) {
		if (path_str[i] == '/') {
			path_str[i] = '\\';
		}
	}

	String16 const path16 = str16_from_8(scratch.arena, path_copy);
	SFGAOF flags = 0;
	PIDLIST_ABSOLUTE list = nullptr;
	if (path16.size > 0 &&
		SUCCEEDED(SHParseDisplayName(reinterpret_cast<WCHAR const *>(path16.data), nullptr, &list, 0, &flags))) {
		HRESULT const hr = SHOpenFolderAndSelectItems(list, 0, nullptr, 0);
		CoTaskMemFree(list);
		(void)hr;
	}
	arena_scratch_end(scratch);
}

namespace {
	constexpr FILEOPENDIALOGOPTIONS WIN32_FILE_DIALOG_DEFAULT_OPTIONS =
		FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;

	auto os_win32_create_filter_specs(
		dk::Arena *arena,
		dk::OS_FileDialogFilter const *filters,
		dk::u64 filter_count,
		COMDLG_FILTERSPEC **out_specs,
		dk::u32 *out_count
	) noexcept -> void {
		using namespace dk;

		if (filter_count == 0) {
			*out_specs = nullptr;
			*out_count = 0;
			return;
		}

		COMDLG_FILTERSPEC *filter_spec = arena_push_type_array<COMDLG_FILTERSPEC>(arena, filter_count + 1);
		String8 const delimiters = str8_literal(",");
		StringJoinParams join_params = {};
		join_params.separator = str8_literal(";");

		// NOTE(Dedrick): IFileDialog expects extension filters in the following format: "*.txt;*.text".
		for (u64 i = 0; i < filter_count; ++i) {
			String8List const extensions = str8_list_split(arena, filters[i].extensions, &delimiters, 1);
			String8List fmt_extensions = {};
			for (String8Node const *node = extensions.first; node != nullptr; node = node->next) {
				str8_list_pushf(arena, &fmt_extensions, "*.%.*s", static_cast<s32>(node->string.size), node->string.data);
			}
			String8 const spec_str = str8_list_join(arena, fmt_extensions, &join_params);
			String8 const display_name = str8f(
				arena,
				"%.*s (%.*s)",
				static_cast<s32>(filters[i].display_name.size), filters[i].display_name.data,
				static_cast<s32>(spec_str.size), spec_str.data);

			filter_spec[i].pszName = reinterpret_cast<WCHAR const *>(str16_from_8(arena, display_name).data);
			filter_spec[i].pszSpec = reinterpret_cast<WCHAR const *>(str16_from_8(arena, spec_str).data);
		}

		filter_spec[filter_count].pszName = L"All Files (*.*)";
		filter_spec[filter_count].pszSpec = L"*.*";

		*out_specs = filter_spec;
		*out_count = static_cast<u32>(filter_count + 1);
	}
}

auto dk::os_file_dialog_pick_file(
	Arena *arena, OS_Handle parent, OS_FileDialogFilter const *filters, u64 filter_count
) noexcept -> String8 {
	using Microsoft::WRL::ComPtr;

	String8 result = {};
	ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(&arena, 1));

	ComPtr<IFileOpenDialog> dialog{};
	if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)))) {
		FILEOPENDIALOGOPTIONS options{};
		dialog->GetOptions(&options);
		dialog->SetOptions(options | WIN32_FILE_DIALOG_DEFAULT_OPTIONS);

		COMDLG_FILTERSPEC *filter_spec = nullptr;
		u32 filter_spec_count = 0;
		os_win32_create_filter_specs(scratch.arena, filters, filter_count, &filter_spec, &filter_spec_count);
		if (filter_spec_count > 0) {
			dialog->SetFileTypes(filter_spec_count, filter_spec);
		}

		OS_Win32_Window const *parent_win = os_win32_window_from_handle(parent);
		HWND const parent_hwnd = parent_win != nullptr ? glfwGetWin32Window(parent_win->glfw_window) : nullptr;
		if (SUCCEEDED(dialog->Show(parent_hwnd))) {
			ComPtr<IShellItem> selected_item{};
			if (SUCCEEDED(dialog->GetResult(&selected_item))) {
				PWSTR path = nullptr;
				if (SUCCEEDED(selected_item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
					if (path != nullptr) {
						String16 const path16 = {
							.data = reinterpret_cast<u16 *>(path),
							.size = static_cast<u64>(lstrlenW(path))
						};
						result = path_normalize_from_str8(arena, str8_from_16(scratch.arena, path16));
						CoTaskMemFree(path);
					}
				}
			}
		}
	}

	arena_scratch_end(scratch);
	return result;
}

auto dk::os_file_dialog_pick_multiple_files(
	Arena *arena, OS_Handle parent, OS_FileDialogFilter const *filters, u64 filter_count
) noexcept -> String8List {
	using Microsoft::WRL::ComPtr;

	String8List result = {};
	ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(&arena, 1));

	ComPtr<IFileOpenDialog> dialog{};
	if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)))) {
		FILEOPENDIALOGOPTIONS options{};
		dialog->GetOptions(&options);
		dialog->SetOptions(options | WIN32_FILE_DIALOG_DEFAULT_OPTIONS | FOS_ALLOWMULTISELECT);

		COMDLG_FILTERSPEC *filter_spec = nullptr;
		u32 filter_spec_count = 0;
		os_win32_create_filter_specs(scratch.arena, filters, filter_count, &filter_spec, &filter_spec_count);
		if (filter_spec_count > 0) {
			dialog->SetFileTypes(filter_spec_count, filter_spec);
		}

		OS_Win32_Window const *parent_win = os_win32_window_from_handle(parent);
		HWND const parent_hwnd = parent_win != nullptr ? glfwGetWin32Window(parent_win->glfw_window) : nullptr;
		if (SUCCEEDED(dialog->Show(parent_hwnd))) {
			ComPtr<IShellItemArray> items{};
			if (SUCCEEDED(dialog->GetResults(&items))) {
				DWORD count = 0;
				items->GetCount(&count);
				for (DWORD i = 0; i < count; ++i) {
					ComPtr<IShellItem> item;
					if (SUCCEEDED(items->GetItemAt(i, &item))) {
						SFGAOF attribs{};
						if (SUCCEEDED(item->GetAttributes(SFGAO_FILESYSTEM, &attribs))) {
							if ((attribs & SFGAO_FILESYSTEM) == 0) {
								continue;
							}
						}

						PWSTR path = nullptr;
						if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
							if (path != nullptr) {
								String16 const path16 = {
									.data = reinterpret_cast<u16 *>(path),
									.size = static_cast<u64>(lstrlenW(path))
								};
								str8_list_push(arena, &result, path_normalize_from_str8(arena, str8_from_16(scratch.arena, path16)));
								CoTaskMemFree(path);
							}
						}
					}
				}
			}
		}
	}

	arena_scratch_end(scratch);
	return result;
}

auto dk::os_file_dialog_save(
	Arena *arena, OS_Handle parent, String8 default_name, OS_FileDialogFilter const *filters, u64 filter_count, u32 *out_filter_index
) noexcept -> String8 {
	using Microsoft::WRL::ComPtr;

	String8 result = {};
	ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(&arena, 1));

	ComPtr<IFileSaveDialog> dialog{};
	if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)))) {
		FILEOPENDIALOGOPTIONS options{};
		dialog->GetOptions(&options);
		dialog->SetOptions(options | WIN32_FILE_DIALOG_DEFAULT_OPTIONS);

		COMDLG_FILTERSPEC *filter_spec = nullptr;
		u32 filter_spec_count = 0;
		os_win32_create_filter_specs(scratch.arena, filters, filter_count, &filter_spec, &filter_spec_count);
		
		if (filter_spec_count > 0) {
			dialog->SetFileTypes(filter_spec_count, filter_spec);
			String8 const delimiters = str8_literal(",");
			String8List const extensions = str8_list_split(scratch.arena, filters[0].extensions, &delimiters, 1);
			if (extensions.first != nullptr) {
				String16 const ext16 = str16_from_8(scratch.arena, extensions.first->string);
				dialog->SetDefaultExtension(reinterpret_cast<WCHAR const *>(ext16.data));
			}
		}

		if (default_name.size > 0) {
			String16 const default_name16 = str16_from_8(scratch.arena, default_name);
			dialog->SetFileName(reinterpret_cast<WCHAR const *>(default_name16.data));
		}

		OS_Win32_Window const *parent_win = os_win32_window_from_handle(parent);
		HWND const parent_hwnd = parent_win != nullptr ? glfwGetWin32Window(parent_win->glfw_window) : nullptr;
		if (SUCCEEDED(dialog->Show(parent_hwnd))) {
			ComPtr<IShellItem> selected_item{};
			if (SUCCEEDED(dialog->GetResult(&selected_item))) {
				UINT file_type_index = 0;
				if (out_filter_index != nullptr && SUCCEEDED(dialog->GetFileTypeIndex(&file_type_index))) {
					*out_filter_index = file_type_index - 1;
				}

				PWSTR path = nullptr;
				if (SUCCEEDED(selected_item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
					if (path != nullptr) {
						String16 const path16 = {
							.data = reinterpret_cast<u16 *>(path),
							.size = static_cast<u64>(lstrlenW(path))
						};
						result = path_normalize_from_str8(arena, str8_from_16(scratch.arena, path16));
						CoTaskMemFree(path);
					}
				}
			}
		}
	}

	arena_scratch_end(scratch);
	return result;
}

auto dk::os_file_dialog_pick_folder(Arena *arena, OS_Handle parent) noexcept -> String8 {
	using Microsoft::WRL::ComPtr;

	String8 result = {};
	ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(&arena, 1));

	ComPtr<IFileOpenDialog> dialog{};
	if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog)))) {
		FILEOPENDIALOGOPTIONS options{};
		dialog->GetOptions(&options);
		dialog->SetOptions(options | WIN32_FILE_DIALOG_DEFAULT_OPTIONS | FOS_PICKFOLDERS);

		OS_Win32_Window const *parent_win = os_win32_window_from_handle(parent);
		HWND const parent_hwnd = parent_win != nullptr ? glfwGetWin32Window(parent_win->glfw_window) : nullptr;
		if (SUCCEEDED(dialog->Show(parent_hwnd))) {
			ComPtr<IShellItem> selected_item{};
			if (SUCCEEDED(dialog->GetResult(&selected_item))) {
				PWSTR path = nullptr;
				if (SUCCEEDED(selected_item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
					if (path != nullptr) {
						String16 const path16 = {
							.data = reinterpret_cast<u16 *>(path),
							.size = static_cast<u64>(lstrlenW(path))
						};
						result = path_normalize_from_str8(arena, str8_from_16(scratch.arena, path16));
						CoTaskMemFree(path);
					}
				}
			}
		}
	}

	arena_scratch_end(scratch);
	return result;
}
