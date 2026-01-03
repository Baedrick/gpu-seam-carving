#pragma once

#include "base/base_arena.hpp"
#include "base/base_math.hpp"
#include "base/base_strings.hpp"
#include "base/base_types.hpp"
#include "os/os_core.hpp"

namespace dk {
	enum class OS_Key : u8;
	enum class OS_MouseButton : u8;

	enum class OS_EventType : u8 {
		NONE,
		WINDOW_CLOSE,
		WINDOW_RESIZE,
		WINDOW_CONTENT_SCALE_CHANGED,
		WINDOW_MINIMIZED,
		WINDOW_MAXIMIZED,
		WINDOW_RESTORED,
		WINDOW_MOUSE_ENTER,
		WINDOW_MOUSE_LEAVE,
		WINDOW_FOCUS_GAINED,
		WINDOW_FOCUS_LOST,
		KEY_DOWN,
		KEY_UP,
		MOUSE_MOTION,
		MOUSE_BUTTON_DOWN,
		MOUSE_BUTTON_UP,
		MOUSE_WHEEL
	};

	struct OS_EventWindow {
		OS_Handle window;
	};

	struct OS_EventWindowResize {
		OS_Handle window;
		s32 width;
		s32 height;
	};

	struct OS_EventWindowContentScale {
		OS_Handle window;
		f32 x_scale;
		f32 y_scale;
	};

	struct OS_EventKey {
		OS_Key key;
	};

	struct OS_EventMouseButton {
		OS_MouseButton button;
	};

	struct OS_EventMouseMove {
		f32 x;
		f32 y;
	};

	struct OS_EventMouseWheel {
		f32 dx;
		f32 dy;
	};

	union OS_EventData {
		OS_EventWindow window_common;
		OS_EventWindowResize window_resize;
		OS_EventWindowContentScale window_content_scale;
		OS_EventKey key;
		OS_EventMouseButton button;
		OS_EventMouseMove mouse_move;
		OS_EventMouseWheel mouse_wheel;
	};

	struct OS_Event {
		OS_Event *next;
		OS_Event *prev;
		OS_EventType type;
		OS_EventData data;
	};

	struct OS_EventList {
		OS_Event *first;
		OS_Event *last;
		u32 count;
	};

	using OS_WindowFlags = u8;
	enum : u8 {
		OS_WINDWOW_FLAG_NONE = 0,
		OS_WINDOW_FLAG_NO_RESIZE = 1u << 0,
		OS_WINDOW_FLAG_CENTER = 1u << 1
	};

	enum class OS_DialogIcon : u8 {
		ICON_INFO,
		ICON_WARNING,
		ICON_ERROR
	};

	struct OS_FileDialogFilter {
		String8 display_name; ///< e.g. "Text files"
		String8 extensions; ///< e.g. "txt,text"
	};

	auto os_gfx_init() noexcept -> void;

	auto os_gfx_shutdown() noexcept -> void;


	auto os_get_events(Arena *arena) noexcept -> OS_EventList;

	auto os_consume_event(OS_EventList *events, OS_Event *event) noexcept -> void;


	auto os_window_open(String8 title, s32 x, s32 y, s32 w, s32 h, OS_WindowFlags flags) noexcept -> OS_Handle;

	auto os_window_close(OS_Handle window) noexcept -> void;

	auto os_window_client_size(OS_Handle window) noexcept -> vec2;
	
	auto os_window_content_scale(OS_Handle window) noexcept -> f32;

	auto os_window_swap_interval(s32 interval) noexcept -> void;
	
	auto os_window_present(OS_Handle window) noexcept -> void;


	auto os_show_dialog(OS_Handle parent, OS_DialogIcon icon, String8 title, String8 message) noexcept -> void;

	auto os_show_in_file_browser(String8 path) noexcept -> void;


	auto os_file_dialog_pick_file(Arena *arena, OS_Handle parent, OS_FileDialogFilter const *filters, u64 filter_count) noexcept -> String8;

	auto os_file_dialog_pick_multiple_files(Arena *arena, OS_Handle parent, OS_FileDialogFilter const *filters, u64 filter_count) noexcept -> String8List;

	auto os_file_dialog_save(Arena *arena, OS_Handle parent, String8 default_name, OS_FileDialogFilter const *filters, u64 filter_count, u32 *out_filter_index) noexcept -> String8;

	auto os_file_dialog_pick_folder(Arena *arena, OS_Handle parent) noexcept -> String8;
}
