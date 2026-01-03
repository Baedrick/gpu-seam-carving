/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

#include "base/base.hpp"
#include "os/os.hpp"
#include "sc/sc_assets.hpp"
#include "sc/sc_imgui.hpp"
#include "sc/sc_opengl.hpp"
#include "thirdparty/argh.h"
#include "thirdparty/stb_image.h"
#include "thirdparty/stb_image_write.h"

using namespace dk;

namespace {
	constexpr s32 REDUCTION_WORKGROUP_SIZE = 256;

	enum SC_Axis : u8 {
		SC_AXIS_VERTICAL = 0,
		SC_AXIS_HORIZONTAL,

		SC_AXIS_MAX_COUNT
	};

	struct SC_Config {
		s32 win_width;
		s32 win_height;
		s32 max_texture_size; ///< Maximum texture size supported on width and height.
	};

	struct SC_SeamPassShaders {
		GLuint prog_cost;
		GLuint prog_find_min_local;
		GLuint prog_find_min_global;
		GLuint prog_backtrace;
		GLuint prog_remove_seam;
	};

	struct SC_GpuResource {
		GLuint empty_vao;

		GLuint time_queries[8];
		b8 time_queries_in_flight[8];

		GLuint tex_scratch[2]; ///< GL_RGBA8
		GLuint tex_original; ///< GL_SRGB8_ALPHA8
		GLuint tex_energy; ///< GL_R32F

		GLuint ubo_display;
		GLuint ubo_carve;
		GLuint ssbo_cost;
		GLuint ssbo_seam;
		GLuint ssbo_min_index; ///< uvec2 = (cost, index)

		GLuint prog_srgb_to_linear;
		GLuint prog_display;
		GLuint prog_sobel;

		SC_SeamPassShaders seam_passes[SC_AXIS_MAX_COUNT];
	};

	using SC_ContextFlags = u32;
	enum : SC_ContextFlags {
		SC_FLAG_NONE = 0,
		SC_FLAG_HAS_IMAGE = 1u << 0,
		SC_FLAG_SHOW_SEAM = 1u << 1,
		SC_FLAG_SEAM_IS_HORIZONTAL = 1u << 2,
		SC_FLAG_SHOW_GUI = 1u << 3,
		SC_FLAG_IS_CARVING = 1u << 4,
		SC_FLAG_PENDING_RESET = 1u << 5,
		SC_FLAG_PENDING_CARVE = 1u << 6,
		SC_FLAG_VSYNC_ENABLED = 1u << 7,
	};

	enum class SC_DebugView : s32 {
		NONE = 0,
		ENERGY
	};

	struct SC_Context {
		Arena *global_arena;
		OS_Handle window;

		SC_GpuResource gpu;

		Arena *image_arena;
		String8 image_path;
		u64 carve_time_us;
		u32 seam_count_vertical;
		u32 seam_count_horizontal;
		
		s32 max_texture_size;
		s32 original_width;
		s32 original_height;
		s32 current_width;
		s32 current_height;
		s32 target_width;
		s32 target_height;

		GLuint tex_src; ///< Points to tex_scratch[0] or tex_scratch[1]
		GLuint tex_dst;

		u64 frame_time_us;
		SC_DebugView current_view;
		SC_ContextFlags flags;

		String8 pending_load_path;
		String8 pending_save_path;
		u32 pending_save_filter_index;

		f32 *plot_history;
		u32 plot_count;
		u32 plot_capacity;
	};
}

namespace {
	auto sc_gpu_alloc(SC_GpuResource *gpu, s32 max_texture_size) noexcept -> void {
		glCreateVertexArrays(1, &gpu->empty_vao);
		glCreateQueries(GL_TIME_ELAPSED, static_cast<GLsizei>(array_size(gpu->time_queries)), gpu->time_queries);
		for (b8 &in_flight : gpu->time_queries_in_flight) {
			in_flight = false;
		}

		gpu->ubo_display = gl_buffer_create(sizeof(SC_DisplayParams), GL_DYNAMIC_STORAGE_BIT, nullptr);
		gpu->ubo_carve = gl_buffer_create(sizeof(SC_CarveParams), GL_DYNAMIC_STORAGE_BIT, nullptr);

		gpu->ssbo_cost = gl_buffer_create(static_cast<u64>(max_texture_size) * max_texture_size * sizeof(f32), 0, nullptr);
		gpu->ssbo_seam = gl_buffer_create(static_cast<u64>(max_texture_size) * sizeof(s32), 0, nullptr);
		gpu->ssbo_min_index = gl_buffer_create(static_cast<u64>(max_texture_size) * sizeof(uvec2), 0, nullptr);

		gpu->tex_scratch[0] = gl_texture_create(GL_RGBA8, max_texture_size, max_texture_size);
		gpu->tex_scratch[1] = gl_texture_create(GL_RGBA8, max_texture_size, max_texture_size);
		gpu->tex_original = gl_texture_create(GL_SRGB8_ALPHA8, max_texture_size, max_texture_size);
		gpu->tex_energy = gl_texture_create(GL_R32F, max_texture_size, max_texture_size);

		gpu->prog_display = gl_program_create(vs_display, fs_display);
		gpu->prog_srgb_to_linear = gl_compute_program_create(cs_srgb_to_linear);
		gpu->prog_sobel = gl_compute_program_create(cs_sobel);

		String8 const compute_shaders[SC_AXIS_MAX_COUNT][5] = {
			{ cs_v_cost_row, cs_v_find_min_local, cs_v_find_min_global, cs_v_backtrace, cs_v_remove_seam },
			{ cs_h_cost_col, cs_h_find_min_local, cs_h_find_min_global, cs_h_backtrace, cs_h_remove_seam },
		};

		for (u32 i = 0; i < SC_AXIS_MAX_COUNT; ++i) {
			gpu->seam_passes[i].prog_cost = gl_compute_program_create(compute_shaders[i][0]);
			gpu->seam_passes[i].prog_find_min_local = gl_compute_program_create(compute_shaders[i][1]);
			gpu->seam_passes[i].prog_find_min_global = gl_compute_program_create(compute_shaders[i][2]);
			gpu->seam_passes[i].prog_backtrace = gl_compute_program_create(compute_shaders[i][3]);
			gpu->seam_passes[i].prog_remove_seam = gl_compute_program_create(compute_shaders[i][4]);
		}
	}

	auto sc_gpu_release(SC_GpuResource *gpu) noexcept -> void {
		for (u32 i = 0; i < SC_AXIS_MAX_COUNT; ++i) {
			gl_program_destroy(gpu->seam_passes[i].prog_remove_seam);
			gl_program_destroy(gpu->seam_passes[i].prog_backtrace);
			gl_program_destroy(gpu->seam_passes[i].prog_find_min_global);
			gl_program_destroy(gpu->seam_passes[i].prog_find_min_local);
			gl_program_destroy(gpu->seam_passes[i].prog_cost);
		}

		gl_program_destroy(gpu->prog_sobel);
		gl_program_destroy(gpu->prog_srgb_to_linear);
		gl_program_destroy(gpu->prog_display);

		gl_texture_destroy(gpu->tex_energy);
		gl_texture_destroy(gpu->tex_original);
		gl_texture_destroy(gpu->tex_scratch[1]);
		gl_texture_destroy(gpu->tex_scratch[0]);

		gl_buffer_destroy(gpu->ssbo_min_index);
		gl_buffer_destroy(gpu->ssbo_seam);
		gl_buffer_destroy(gpu->ssbo_cost);

		gl_buffer_destroy(gpu->ubo_carve);
		gl_buffer_destroy(gpu->ubo_display);

		glDeleteQueries(static_cast<GLsizei>(array_size(gpu->time_queries)), gpu->time_queries);
		glDeleteVertexArrays(1, &gpu->empty_vao);
	}

	auto sc_create(SC_Config const *cfg) noexcept -> SC_Context * {
		constexpr ArenaParams params = {
			.reserve_size = ARENA_DEFAULT_RESERVE_SIZE,
			.commit_size = ARENA_DEFAULT_COMMIT_SIZE
		};
		Arena *global_arena = arena_alloc(&params);
		Arena *image_arena = arena_alloc(&params);

		SC_Context *sc = arena_push_type<SC_Context>(global_arena);
		sc->global_arena = global_arena;
		sc->image_arena = image_arena;

		OS_Handle const window = os_window_open(
			str8_literal("Parallelized Seam Carving (GPU Compute)"),
			0, 0, cfg->win_width, cfg->win_height,
			OS_WINDOW_FLAG_CENTER
		);
		if (window == os_handle_invalid()) {
			arena_release(global_arena);
			return nullptr;
		}
		sc->window = window;

		gladLoaderLoadGL();

#ifndef NDEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(gl_debug_callback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif

		sc_gpu_alloc(&sc->gpu, cfg->max_texture_size);
		imgui_init(window);

		stbi_set_flip_vertically_on_load(true);
		stbi_flip_vertically_on_write(true);

		sc->max_texture_size = cfg->max_texture_size;
		sc->tex_src = sc->gpu.tex_scratch[0];
		sc->tex_dst = sc->gpu.tex_scratch[1];
		sc->current_view = SC_DebugView::NONE;
		sc->flags = SC_FLAG_SHOW_GUI | SC_FLAG_VSYNC_ENABLED;
		os_window_swap_interval(1);
		sc->plot_capacity = static_cast<u32>(cfg->max_texture_size) * 2;
		sc->plot_history = arena_push_type_array<f32>(global_arena, sc->plot_capacity);

		return sc;
	}

	auto sc_destroy(SC_Context *sc) noexcept -> void {
		imgui_shutdown();
		sc_gpu_release(&sc->gpu);
		os_window_close(sc->window);
		arena_release(sc->global_arena);
	}

	auto sc_update_carve_params(SC_Context *sc, s32 current_iteration) noexcept -> void {
		SC_CarveParams const params = {
			.current_size = { sc->current_width, sc->current_height },
			.texture_size = { sc->max_texture_size, sc->max_texture_size },
			.current_iteration = current_iteration
		};
		glNamedBufferSubData(sc->gpu.ubo_carve, 0, sizeof(SC_CarveParams), &params);
	}

	auto sc_reset_image(SC_Context *sc) noexcept -> void {
		if ((sc->flags & SC_FLAG_HAS_IMAGE) == 0) {
			return;
		}

		sc->current_width = sc->original_width;
		sc->current_height = sc->original_height;
		sc->seam_count_vertical = 0;
		sc->seam_count_horizontal = 0;
		sc->carve_time_us = 0;
		sc->plot_count = 0;
		sc->flags &= ~SC_FLAG_IS_CARVING;

		glUseProgram(sc->gpu.prog_srgb_to_linear);
		sc_update_carve_params(sc, 0);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, sc->gpu.ubo_carve);
		glBindTextureUnit(0, sc->gpu.tex_original);
		glBindImageTexture(0, sc->gpu.tex_scratch[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
		glDispatchCompute((sc->original_width + 7) / 8, (sc->original_height + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		sc->tex_src = sc->gpu.tex_scratch[0];
		sc->tex_dst = sc->gpu.tex_scratch[1];

		constexpr s32 clear_value = -1;
		glClearNamedBufferData(sc->gpu.ssbo_seam, GL_R32I, GL_RED_INTEGER, GL_INT, &clear_value);
	}

	auto sc_start_carve(SC_Context *sc) noexcept -> void {
		sc->seam_count_vertical = 0;
		sc->seam_count_horizontal = 0;
		sc->carve_time_us = 0;
		sc->flags |= SC_FLAG_IS_CARVING;

		for (b8 &in_flight : sc->gpu.time_queries_in_flight) {
			in_flight = false;
		}
	}

	auto sc_carve_seam(SC_Context *sc, SC_Axis axis) noexcept -> void {
		s32 const width = sc->current_width;
		s32 const height = sc->current_height;
		s32 const major_dim = axis == SC_AXIS_VERTICAL ? width : height;
		s32 const minor_dim = axis == SC_AXIS_VERTICAL ? height : width;

		glBindBufferBase(GL_UNIFORM_BUFFER, 0, sc->gpu.ubo_carve);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sc->gpu.ssbo_cost);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sc->gpu.ssbo_min_index);

		SC_SeamPassShaders const *passes = &sc->gpu.seam_passes[static_cast<u32>(axis)];

		// NOTE(Dedrick): Sobel energy calculation.
		glUseProgram(sc->gpu.prog_sobel);
		sc_update_carve_params(sc, 0);
		glBindTextureUnit(0, sc->tex_src);
		glBindImageTexture(0, sc->gpu.tex_energy, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
		glDispatchCompute((width + 7) / 8, (height + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		// NOTE(Dedrick): Cost map (DP).
		glUseProgram(passes->prog_cost);
		glBindTextureUnit(1, sc->gpu.tex_energy);
		for (s32 i = 0; i < minor_dim; ++i) {
			sc_update_carve_params(sc, i);
			glDispatchCompute((major_dim + REDUCTION_WORKGROUP_SIZE - 1) / REDUCTION_WORKGROUP_SIZE, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// NOTE(Dedrick): Find minimum seam (2-pass reduction).
		s32 const num_groups = (major_dim + REDUCTION_WORKGROUP_SIZE - 1) / REDUCTION_WORKGROUP_SIZE;
		glUseProgram(passes->prog_find_min_local);
		sc_update_carve_params(sc, 0);
		glDispatchCompute(num_groups, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(passes->prog_find_min_global);
		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// NOTE(Dedrick): Seam back-tracing.
		glUseProgram(passes->prog_backtrace);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sc->gpu.ssbo_seam);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sc->gpu.ssbo_min_index);
		for (s32 i = minor_dim - 1; i >= 0; --i) {
			sc_update_carve_params(sc, i);
			glDispatchCompute(1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// NOTE(Dedrick): Remove seam.
		glUseProgram(passes->prog_remove_seam);
		sc_update_carve_params(sc, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sc->gpu.ssbo_seam);
		glBindImageTexture(0, sc->tex_src, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
		glBindImageTexture(1, sc->tex_dst, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

		s32 const dispatch_w = axis == SC_AXIS_VERTICAL ? width - 1 : width;
		s32 const dispatch_h = axis == SC_AXIS_VERTICAL ? height : height - 1;
		glDispatchCompute((dispatch_w + 7) / 8, (dispatch_h + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		swap(&sc->tex_src, &sc->tex_dst);
		if (axis == SC_AXIS_VERTICAL) {
			sc->current_width -= 1;
		}
		else {
			sc->current_height -= 1;
		}
	}

	auto sc_load_image_from_file(SC_Context *sc, String8 file_path) noexcept -> void {
		s32 width = 0;
		s32 height = 0;
		s32 channels = 0;
		u8 *const data = stbi_load(reinterpret_cast<char const *>(file_path.data), &width, &height, &channels, 4);
		if (data == nullptr) {
			ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(nullptr, 0));
			String8 const msg = str8f(
				scratch.arena,
				"Failed to load image: %s",
				reinterpret_cast<char const *>(file_path.data)
			);
			os_show_dialog(sc->window, OS_DialogIcon::ICON_ERROR, str8_literal("Error"), msg);
			arena_scratch_end(scratch);
			return;
		}

		if (width > sc->max_texture_size || height > sc->max_texture_size) {
			ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(nullptr, 0));
			String8 const msg = str8f(
				scratch.arena,
				"Image too large (%dx%d). Max supported is %dx%d.",
				width, height, sc->max_texture_size, sc->max_texture_size
			);
			os_show_dialog(sc->window, OS_DialogIcon::ICON_ERROR, str8_literal("Error"), msg);
			arena_scratch_end(scratch);
			stbi_image_free(data);
			return;
		}

		glTextureSubImage2D(sc->gpu.tex_original, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);

		arena_clear(sc->image_arena);
		sc->image_path = str8_copy(sc->image_arena, file_path);
		sc->original_width = width;
		sc->original_height = height;
		sc->target_width = width;
		sc->target_height = height;
		sc->flags |= SC_FLAG_HAS_IMAGE;
		sc_reset_image(sc);
	}

	auto sc_linear_to_srgb(f32 c) noexcept -> u8 {
		c = (c <= 0.0031308f) ? (c * 12.92f) : (1.055f * glm::pow(c, 1.0f / 2.4f) - 0.055f);
		return static_cast<u8>(glm::clamp(c, 0.0f, 1.0f) * 255.0f);
	}

	auto sc_save_image_to_file(SC_Context *sc, String8 file_path, u32 filter_index) noexcept -> void {
		s32 const width = sc->current_width;
		s32 const height = sc->current_height;
		u64 const byte_count = static_cast<u64>(width) * height * 4;

		// NOTE(Dedrick): Image size can be huge. It's better to allocate specifically
		// for these images and free the memory after to keep program memory usage low.
		// NOTE(Dedrick): Unlikely that the user will save images out. Migrate to GPU
		// implementation only if it is a frequent action taken by the user.

		u8 *const linear_data = static_cast<u8 *>(std::malloc(byte_count * 2));
		u8 *const srgb_data = linear_data + byte_count;

		glGetTextureSubImage(
			sc->tex_src,
			0,
			0, 0, 0,
			width, height, 1,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			static_cast<GLsizei>(byte_count),
			linear_data
		);

		for (usize i = 0; i < byte_count; i += 4) {
			srgb_data[i + 0] = sc_linear_to_srgb(static_cast<f32>(linear_data[i + 0]) / 255.0f);
			srgb_data[i + 1] = sc_linear_to_srgb(static_cast<f32>(linear_data[i + 1]) / 255.0f);
			srgb_data[i + 2] = sc_linear_to_srgb(static_cast<f32>(linear_data[i + 2]) / 255.0f);
			srgb_data[i + 3] = linear_data[i + 3];
		}

		if (filter_index == 1) {
			stbi_write_jpg(reinterpret_cast<char const *>(file_path.data), width, height, 4, srgb_data, 90);
		} else {
			stbi_write_png(reinterpret_cast<char const *>(file_path.data), width, height, 4, srgb_data, width * 4);
		}

		std::free(linear_data);
	}

	auto sc_gui(SC_Context *sc, Arena *frame_arena) noexcept -> void {
		if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
			sc->flags ^= SC_FLAG_SHOW_GUI;
		}

		if ((sc->flags & SC_FLAG_SHOW_GUI) == 0) {
			return;
		}

		b8 const has_image = (sc->flags & SC_FLAG_HAS_IMAGE) != 0;
		b8 const is_carving = (sc->flags & SC_FLAG_IS_CARVING) != 0;

		ImGui::Begin("Seam Carver");
		if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
			f32 const frame_time_ms = static_cast<f32>(sc->frame_time_us) / 1000.0f;
			ImGui::Text("FPS (dt): %.1f (%.2f ms)", 1000.0f / frame_time_ms, frame_time_ms);
			if (ImGui::CheckboxFlags("VSync", &sc->flags, SC_FLAG_VSYNC_ENABLED)) {
				os_window_swap_interval((sc->flags & SC_FLAG_VSYNC_ENABLED) ? 1 : 0);
			}
			u32 const total_seam_count = sc->seam_count_vertical + sc->seam_count_horizontal;
			if (total_seam_count > 0) {
				f32 const total_carve_time_ms = static_cast<f32>(sc->carve_time_us) / 1000.0f;
				ImGui::Text("Total Carve Time: %.2f ms (%u seams)", total_carve_time_ms, total_seam_count);
				ImGui::Text("Vertical Seams: %u", sc->seam_count_vertical);
				ImGui::Text("Horizontal Seams: %u", sc->seam_count_horizontal);
				ImGui::Text("Average Seam Time: %.4f ms", total_carve_time_ms / static_cast<f32>(total_seam_count));

				ImGui::Separator();
				ImGui::Text("Compute Time (ms) vs Seams Removed");
				if (sc->plot_count > 0) {
					ImGui::PlotLines(
						"##ComputeTime",
						sc->plot_history,
						static_cast<int>(sc->plot_count),
						0,
						nullptr,
						0.0f,
						FLT_MAX,
						ImVec2(0, 100)
					);
				}
			} else {
				ImGui::Text("Run a carving operation to see performance.");
			}
		}
		if (ImGui::CollapsingHeader("File", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Button("Load Image")) {
				OS_FileDialogFilter const image_filters[] = {
					{ .display_name = str8_literal("Image files"), .extensions = str8_literal("png,jpg,jpeg") }
				};
				String8 const file_path = os_file_dialog_pick_file(
					frame_arena,
					sc->window,
					image_filters,
					array_size(image_filters)
				);
				if (file_path.size > 0) {
					sc->pending_load_path = file_path;
				}
			}
			ImGui::SameLine();
			if (!has_image) { ImGui::PushDisabled(); }
			if (ImGui::Button("Save Image")) {
				OS_FileDialogFilter const image_filters[] = {
					{ .display_name = str8_literal("PNG files"), .extensions = str8_literal("png") },
					{ .display_name = str8_literal("JPEG files"), .extensions = str8_literal("jpg,jpeg") },
				};
				u32 filter_index = 0;
				String8 const file_path = os_file_dialog_save(
					frame_arena,
					sc->window, str8_literal("image"),
					image_filters,
					array_size(image_filters),
					&filter_index
				);
				if (file_path.size > 0) {
					sc->pending_save_path = file_path;
					sc->pending_save_filter_index = filter_index;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset Image")) {
				sc->flags |= SC_FLAG_PENDING_RESET;
			}
			if (!has_image) { ImGui::PopDisabled(); }
			ImGui::Text(
				"Current: %s",
				sc->image_path.size > 0 ? reinterpret_cast<char const *>(sc->image_path.data) : "No image loaded"
			);
		}
		if (ImGui::CollapsingHeader("Carving", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (!has_image) { ImGui::PushDisabled(); }
			
			ImGui::Text("Original: %d x %d", sc->original_width, sc->original_height);
			ImGui::Text("Current:  %d x %d", sc->current_width, sc->current_height);

			if (is_carving) { ImGui::PushDisabled(); }
			ImGui::SliderInt("Target Width", &sc->target_width, 1, sc->original_width);
			ImGui::SliderInt("Target Height", &sc->target_height, 1, sc->original_height);
			if (is_carving) { ImGui::PopDisabled(); }

			b8 const can_carve =
				(sc->target_width != sc->current_width || sc->target_height != sc->current_height) && !is_carving;
			if (!can_carve) { ImGui::PushDisabled(); }
			if (ImGui::Button("Carve")) {
				if (sc->target_width > sc->current_width || sc->target_height > sc->current_height) {
					sc->flags |= SC_FLAG_PENDING_RESET;
				}
				sc->flags |= SC_FLAG_PENDING_CARVE;
			}
			if (!can_carve) { ImGui::PopDisabled(); }
			if ((sc->flags & SC_FLAG_IS_CARVING) != 0) {
				ImGui::Text("Carving...");
			}
			if (!has_image) { ImGui::PopDisabled(); }
		}
		if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (!has_image) { ImGui::PushDisabled(); }
			s32 *view_mode = reinterpret_cast<s32 *>(&sc->current_view);
			char const *view_mode_names[] = { "NONE", "ENERGY" };
			ImGui::Combo("Debug View", view_mode, view_mode_names, static_cast<int>(array_size(view_mode_names)));
			ImGui::CheckboxFlags("Show Seam", &sc->flags, SC_FLAG_SHOW_SEAM);
			if (!has_image) { ImGui::PopDisabled(); }
		}

		ImGui::End();
	}

	auto sc_update_carving(SC_Context *sc) noexcept -> void {
		if ((sc->flags & SC_FLAG_IS_CARVING) == 0) {
			return;
		}

		for (s32 i = 0; i < static_cast<s32>(array_size(sc->gpu.time_queries)); ++i) {
			if (sc->gpu.time_queries_in_flight[i]) {
				GLint query_ready = 0;
				glGetQueryObjectiv(sc->gpu.time_queries[i], GL_QUERY_RESULT_AVAILABLE, &query_ready);
				if (query_ready) {
					GLint64 time_ns = 0;
					glGetQueryObjecti64v(sc->gpu.time_queries[i], GL_QUERY_RESULT, &time_ns);
					sc->carve_time_us += time_ns / 1000;
					sc->gpu.time_queries_in_flight[i] = false;

					// NOTE(Dedrick): The values will be out of order
					// but as a plot they will paint the right picture.
					if (sc->plot_count < sc->plot_capacity) {
						sc->plot_history[sc->plot_count++] = static_cast<f32>(time_ns) / 1000000.0f;
					}
				}
			}
		}

		b8 const needs_carve = sc->current_width > sc->target_width || sc->current_height > sc->target_height;
		if (!needs_carve) {
			sc->flags &= ~SC_FLAG_IS_CARVING;
			// NOTE(Dedrick): A few dropped queries is fine.
			for (u32 i = 0; i < array_size(sc->gpu.time_queries); ++i) {
				if (sc->gpu.time_queries_in_flight[i]) {
					GLint64 time_ns = 0;
					glGetQueryObjecti64v(sc->gpu.time_queries[i], GL_QUERY_RESULT, &time_ns);
					sc->carve_time_us += time_ns / 1000;
					sc->gpu.time_queries_in_flight[i] = false;

					// NOTE(Dedrick): The values will be out of order
					// but as a plot they will paint the right picture.
					if (sc->plot_count < sc->plot_capacity) {
						sc->plot_history[sc->plot_count++] = static_cast<f32>(time_ns) / 1000000.0f;
					}
				}
			}
			return;
		}

		s32 available_query_slot = -1;
		for (s32 i = 0; i < static_cast<s32>(array_size(sc->gpu.time_queries)); ++i) {
			if (!sc->gpu.time_queries_in_flight[i]) {
				available_query_slot = i;
				break;
			}
		}

		if (available_query_slot >= 0) {
			glBeginQuery(GL_TIME_ELAPSED, sc->gpu.time_queries[available_query_slot]);
		}

		if (sc->current_width > sc->target_width) {
			sc->flags &= ~SC_FLAG_SEAM_IS_HORIZONTAL;
			sc_carve_seam(sc, SC_AXIS_VERTICAL);
			++sc->seam_count_vertical;
		}
		if (sc->current_height > sc->target_height) {
			sc->flags |= SC_FLAG_SEAM_IS_HORIZONTAL;
			sc_carve_seam(sc, SC_AXIS_HORIZONTAL);
			++sc->seam_count_horizontal;
		}

		if (available_query_slot >= 0) {
			glEndQuery(GL_TIME_ELAPSED);
			sc->gpu.time_queries_in_flight[available_query_slot] = true;
		}
	}

	auto sc_run(SC_Context *sc) noexcept -> void {
		u64 last_time_us = os_now_microseconds();
		for (b8 want_quit = false; !want_quit; ) {
			u64 const current_time_us = os_now_microseconds();
			sc->frame_time_us = current_time_us - last_time_us;
			last_time_us = current_time_us;

			ScratchArena const scratch = arena_scratch_begin(tc_get_scratch(nullptr, 0));
			OS_EventList const events = os_get_events(scratch.arena);

			f32 const content_scale = os_window_content_scale(sc->window);
			vec2 const fb_size = content_scale * os_window_client_size(sc->window);

			imgui_new_frame(content_scale);
			sc_gui(sc, scratch.arena);
			imgui_prepare_frame();

			if (sc->pending_load_path.size > 0) {
				sc_load_image_from_file(sc, sc->pending_load_path);
				sc->pending_load_path = {};
			}

			if (sc->pending_save_path.size > 0) {
				sc_save_image_to_file(sc, sc->pending_save_path, sc->pending_save_filter_index);
				sc->pending_save_path = {};
			}

			if ((sc->flags & SC_FLAG_PENDING_RESET) != 0) {
				sc_reset_image(sc);
				sc->flags &= ~SC_FLAG_PENDING_RESET;
			}

			if ((sc->flags & SC_FLAG_PENDING_CARVE) != 0) {
				sc_start_carve(sc);
				sc->flags &= ~SC_FLAG_PENDING_CARVE;
			}
			
			sc_update_carving(sc);

			glViewport(0, 0, static_cast<GLsizei>(fb_size.x), static_cast<GLsizei>(fb_size.y));
			glClear(GL_COLOR_BUFFER_BIT);

			if (sc->current_width > 0 || sc->current_height > 0) {
				if (sc->current_view == SC_DebugView::ENERGY) {
					glUseProgram(sc->gpu.prog_sobel);
					sc_update_carve_params(sc, 0);
					glBindBufferBase(GL_UNIFORM_BUFFER, 0, sc->gpu.ubo_carve);
					glBindTextureUnit(0, sc->tex_src);
					glBindImageTexture(0, sc->gpu.tex_energy, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
					glDispatchCompute((sc->current_width + 7) / 8, (sc->current_height + 7) / 8, 1);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
				}

				glUseProgram(sc->gpu.prog_display);

				SC_DisplayParams params{};
				params.window_size = fb_size;
				params.image_size = { sc->current_width, sc->current_height };
				params.texture_size = { sc->max_texture_size, sc->max_texture_size };
				params.debug_view_mode = static_cast<s32>(sc->current_view);
				params.show_seam = (sc->flags & SC_FLAG_SHOW_SEAM) != 0;
				params.is_horizontal = (sc->flags & SC_FLAG_SEAM_IS_HORIZONTAL) != 0;
				glNamedBufferSubData(sc->gpu.ubo_display, 0, sizeof(SC_DisplayParams), &params);

				glBindBufferBase(GL_UNIFORM_BUFFER, 0, sc->gpu.ubo_display);
				glBindTextureUnit(0, sc->tex_src);

				if (sc->current_view == SC_DebugView::ENERGY) {
					glBindTextureUnit(1, sc->gpu.tex_energy);
				}
				if ((sc->flags & SC_FLAG_SHOW_SEAM) != 0) {
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sc->gpu.ssbo_seam);
				}

				glBindVertexArray(sc->gpu.empty_vao);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}

			imgui_render_frame();
			os_window_present(sc->window);

			for (OS_Event const *event = events.first; event != nullptr; event = event->next) {
				if (event->type == OS_EventType::WINDOW_CLOSE) {
					want_quit = true;
				}
			}
			arena_scratch_end(scratch);
		}
	}
}

extern auto entry_point(int argc, char **argv) noexcept -> int {
	argh::parser opts{};
	opts.add_params({
		"-W", "--width",
		"-H", "--height",
		"-m", "--max-image-size",
	});
	opts.parse(argc, argv);

	if (opts[{ "-h", "--help" }]) {
		std::printf(
			"\nUsage: %s [options]\n"
			"Options:\n"
			"  -h, --help                  Show this help message.\n"
			"  -W, --width <int>           Window width (default: 800).\n"
			"  -H, --height <int>          Window height (default: 600).\n"
			"  -m, --max-image-size <int>  Maximum image size (default: 4096).\n",
			argv[0]
		);
		return 0;
	}

	SC_Config cfg{};
	opts({ "-W", "--width" }, 800) >> cfg.win_width;
	opts({ "-H", "--height" }, 600) >> cfg.win_height;
	opts({ "-m", "--max-image-size" }, 4096) >> cfg.max_texture_size;

	os_gfx_init();
	SC_Context *sc = sc_create(&cfg);
	if (sc == nullptr) {
		os_gfx_shutdown();
		return 1;
	}

	sc_run(sc);
	sc_destroy(sc);
	os_gfx_shutdown();
	return 0;
}
