/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

#pragma once

#include "base/base_math.hpp"
#include "base/base_strings.hpp"

namespace dk {
	struct SC_DisplayParams {
		alignas(8) ivec2 window_size;
		alignas(8) ivec2 image_size;
		alignas(8) ivec2 texture_size;
		alignas(4) s32 debug_view_mode; ///< 0: None, 1: Energy
		alignas(4) s32 show_seam; ///< 0: false, 1: true
		alignas(4) s32 is_horizontal; ///< 0: false, 1: true (for seam drawing)
	};

	struct SC_CarveParams {
		alignas(8) ivec2 current_size;
		alignas(8) ivec2 texture_size;
		alignas(4) s32 current_iteration;
	};

	extern String8 const vs_display;
	extern String8 const fs_display;

	extern String8 const cs_srgb_to_linear;
	extern String8 const cs_sobel;

	extern String8 const cs_v_cost_row;
	extern String8 const cs_v_find_min_local;
	extern String8 const cs_v_find_min_global;
	extern String8 const cs_v_backtrace;
	extern String8 const cs_v_remove_seam;

	extern String8 const cs_h_cost_col;
	extern String8 const cs_h_find_min_local;
	extern String8 const cs_h_find_min_global;
	extern String8 const cs_h_backtrace;
	extern String8 const cs_h_remove_seam;
}
