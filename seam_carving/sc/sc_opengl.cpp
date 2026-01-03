/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

#include "sc_opengl.hpp"

#include "base/base_assert.h"
#include "base/base_utils.hpp"

#include <cstdio>

auto dk::gl_buffer_create(u64 size, GLbitfield flags, void const *data) noexcept -> GLuint {
	GLuint buffer = 0;
	glCreateBuffers(1, &buffer);
	glNamedBufferStorage(buffer, static_cast<GLsizeiptr>(size), data, flags);
	return buffer;
}

auto dk::gl_buffer_destroy(GLuint buffer) noexcept -> void {
	glDeleteBuffers(1, &buffer);
}

auto dk::gl_texture_create(GLenum format, s32 width, s32 height) noexcept -> GLuint {
	GLuint texture = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureStorage2D(texture, 1, format, width, height);
	glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return texture;
}

auto dk::gl_texture_destroy(GLuint texture) noexcept -> void {
	glDeleteTextures(1, &texture);
}

auto dk::gl_compile_shader_stage(String8 source, GLenum type) noexcept -> GLuint {
	DK_ASSERT(source.data != nullptr && source.size > 0);

	char const *src = reinterpret_cast<char const *>(source.data);
	s32 const len = static_cast<s32>(source.size);

	GLuint const shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, &len);
	glCompileShader(shader);

	GLint success = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		char info_log[1024];
		glGetShaderInfoLog(shader, static_cast<GLsizei>(array_size(info_log)), nullptr, info_log);

		char const *shader_type_str = "unknown";
		if (type == GL_VERTEX_SHADER) { shader_type_str = "vertex"; }
		if (type == GL_FRAGMENT_SHADER) { shader_type_str = "fragment"; }
		if (type == GL_COMPUTE_SHADER) { shader_type_str = "compute"; }

		(void)std::fprintf(stderr, "Shader [%s] compilation failed\n%s", shader_type_str, info_log);

		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

auto dk::gl_link_shader_programs(GLuint const *shaders, u32 shader_count) noexcept -> GLuint {
	DK_ASSERT(shaders != nullptr && shader_count > 0);

	GLuint const program = glCreateProgram();
	for (u32 i = 0; i < shader_count; ++i) {
		glAttachShader(program, shaders[i]);
	}
	glLinkProgram(program);

	GLint success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success != GL_TRUE) {
		char info_log[1024];
		glGetProgramInfoLog(program, static_cast<GLsizei>(array_size(info_log)), nullptr, info_log);
		(void)std::fprintf(stderr, "Program Linking failed\n%s", info_log);

		glDeleteProgram(program);
		return 0;
	}
	return program;
}

auto dk::gl_compute_program_create(String8 compute_source) noexcept -> GLuint {
	GLuint const cs = gl_compile_shader_stage(compute_source, GL_COMPUTE_SHADER);
	DK_ASSERT(cs != 0);
	GLuint const program = gl_link_shader_programs(&cs, 1);
	DK_ASSERT(program != 0);
	glDeleteShader(cs);
	return program;
}

auto dk::gl_program_create(String8 vertex_source, String8 fragment_source) noexcept -> GLuint {
	GLuint const vs = gl_compile_shader_stage(vertex_source, GL_VERTEX_SHADER);
	DK_ASSERT(vs != 0);
	GLuint const fs = gl_compile_shader_stage(fragment_source, GL_FRAGMENT_SHADER);
	DK_ASSERT(fs != 0);
	GLuint const shaders[2] = { vs, fs };
	GLuint const program = gl_link_shader_programs(shaders, 2);
	DK_ASSERT(program != 0);
	glDeleteShader(vs);
	glDeleteShader(fs);
	return program;
}

auto dk::gl_program_destroy(GLuint program) noexcept -> void {
	glDeleteProgram(program);
}

auto dk::gl_debug_callback(
	GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const *message, void const *user
) noexcept -> void {
	(void)length;
	(void)user;

	// NOTE(Dedrick): Ignore non-significant noisy error codes from NVIDIA drivers.
	if (id == 0x20071 /* Buffer object will use VIDEO memory... */ ||
		id == 0x20081 /* The driver allocated storage for render buffer... */ ||
		id == 0x200B2 /* Shader in program is being recompiled based on GL state... */ ||
		id == 0x200A4 /* Texture object is incurring a software fallback... */) {
		return;
	}

	const char *source_str = nullptr;
	switch (source) {
		case GL_DEBUG_SOURCE_API: source_str = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: source_str = "Window System"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: source_str = "Shader Compiler"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY: source_str = "Third Party"; break;
		case GL_DEBUG_SOURCE_APPLICATION: source_str = "Application"; break;
		case GL_DEBUG_SOURCE_OTHER: source_str = "Other"; break;
		default: source_str = "Unknown"; break;
	}

	const char *type_str = nullptr;
	switch (type) {
		case GL_DEBUG_TYPE_ERROR: type_str = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "Deprecated Behavior"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: type_str = "Undefined Behavior"; break;
		case GL_DEBUG_TYPE_PORTABILITY: type_str = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE: type_str = "Performance"; break;
		case GL_DEBUG_TYPE_MARKER: type_str = "Marker"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP: type_str = "Push Group"; break;
		case GL_DEBUG_TYPE_POP_GROUP: type_str = "Pop Group"; break;
		case GL_DEBUG_TYPE_OTHER: type_str = "Other"; break;
		default: type_str = "Unknown"; break;
	}

	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			(void)std::fprintf(stderr, "OpenGL Error: [%s] from %s -> %s.\n", type_str, source_str, message);
			DK_ASSERT(false);
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			(void)std::fprintf(stderr, "OpenGL Warning: [%s] from %s -> %s.\n", type_str, source_str, message);
			break;
		case GL_DEBUG_SEVERITY_LOW:
			(void)std::fprintf(stderr, "OpenGL Info: [%s] from %s -> %s.\n", type_str, source_str, message);
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			(void)std::fprintf(stderr, "OpenGL Trace: [%s] from %s -> %s.\n", type_str, source_str, message);
			break;
		default: break;
	}
}
