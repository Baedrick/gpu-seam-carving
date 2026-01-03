#pragma once

#include "os/os_gfx.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#undef APIENTRY

namespace dk {
	struct OS_Win32_Window {
		GLFWwindow *glfw_window;
	};

	struct OS_Win32_GfxContext {
		Arena *arena;
		OS_Win32_Window *window;
		Arena *active_events_arena; ///< User provided.
		OS_EventList *active_events_list;
		Arena *pending_events_arena;
		OS_EventList pending_events_list;
		OS_Key key_from_glfw_key_table[GLFW_KEY_LAST + 1];
		OS_MouseButton button_from_glfw_button_table[GLFW_MOUSE_BUTTON_LAST + 1];
	};
	extern OS_Win32_GfxContext *os_win32_gfx_context;

	auto os_win32_handle_from_window(OS_Win32_Window *window) noexcept -> OS_Handle;

	auto os_win32_window_from_handle(OS_Handle handle) noexcept -> OS_Win32_Window *;
}