/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

#include "sc_imgui.hpp"

#include "base/base_assert.h"
#include "os/os_gfx.hpp"
#include "os/os_gfx_win32.hpp"

auto dk::imgui_init(OS_Handle window) noexcept -> void {
	DK_ASSERT(window != os_handle_invalid());

	OS_Win32_Window const *win32_window = os_win32_window_from_handle(window);
	GLFWwindow *glfw_window = win32_window->glfw_window;
	DK_ASSERT(glfw_window != nullptr);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();
	ImGuiStyle &style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg].w = 0.6f;

	ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
	ImGui_ImplOpenGL3_Init();
}

auto dk::imgui_shutdown() noexcept -> void {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
