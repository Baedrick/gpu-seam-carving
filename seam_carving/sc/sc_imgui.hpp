/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

#pragma once

#include "base/base_math.hpp"
#include "base/base_types.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

namespace ImGui {
	inline void PushDisabled() {
		PushItemFlag(ImGuiItemFlags_Disabled, true);
		PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
	}

	inline void PopDisabled() {
		PopItemFlag();
		PopStyleVar();
	}
}

namespace dk {
	struct OS_Handle;

	auto imgui_init(OS_Handle window) noexcept -> void;

	auto imgui_shutdown() noexcept -> void;

	inline auto imgui_new_frame(f32 content_scale) noexcept -> void {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (content_scale <= 0.0f) {
			content_scale = 1.0f;
		}

		ImGuiIO &io = ImGui::GetIO();
		f32 const old_scale = io.FontGlobalScale;
		if (glm::epsilonNotEqual(content_scale, old_scale, CMP_EPSILON)) {
			io.FontGlobalScale = content_scale;
			ImGui::GetStyle().ScaleAllSizes(content_scale / old_scale);
		}
	}

	inline auto imgui_prepare_frame() noexcept -> void {
		ImGui::Render();
	}

	inline auto imgui_render_frame() noexcept -> void {
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}
