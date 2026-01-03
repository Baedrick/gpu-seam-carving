/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

#pragma once

#include "base/base_strings.hpp"

#include <glad/gl.h>

namespace dk {
	auto gl_buffer_create(u64 size, GLbitfield flags, void const *data) noexcept -> GLuint ;

	auto gl_buffer_destroy(GLuint buffer) noexcept -> void ;

	auto gl_texture_create(GLenum format, s32 width, s32 height) noexcept -> GLuint ;

	auto gl_texture_destroy(GLuint texture) noexcept -> void ;

	auto gl_compile_shader_stage(String8 source, GLenum type) noexcept -> GLuint;

	auto gl_link_shader_programs(GLuint const *shaders, u32 shader_count) noexcept -> GLuint ;

	auto gl_compute_program_create(String8 compute_source) noexcept -> GLuint ;

	auto gl_program_create(String8 vertex_source, String8 fragment_source) noexcept -> GLuint ;

	auto gl_program_destroy(GLuint program) noexcept -> void ;

	auto gl_debug_callback(
		GLenum source,
		GLenum type,
		GLuint id,
		GLenum severity,
		GLsizei length,
		GLchar const *message,
		void const *user
	) noexcept -> void;
}
