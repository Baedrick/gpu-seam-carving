/*
 * Copyright (C) 2025 Koh Swee Teck Dedrick.
 * Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
 */

#include "sc_assets.hpp"

namespace dk {
	String8 const vs_display = str8_literal(R"(
#version 460 core

out v2f {
	vec2 texcoord_0;
} Out;

void main() {
	const vec2 pos[3] = vec2[3](
		vec2(-1.0f, -1.0f), // bottom-left
		vec2( 3.0f, -1.0f), // top-left
		vec2(-1.0f,  3.0f)  // bottom-right
	);
	gl_Position = vec4(pos[gl_VertexID], 0.0f, 1.0f);
	Out.texcoord_0 = (pos[gl_VertexID] + vec2(1.0f)) * 0.5f;
}
)");

	String8 const fs_display = str8_literal(R"(
#version 460 core
layout (location = 0) out vec4 out_color;

in v2f {
	vec2 texcoord_0;
} In;

layout (binding = 0) uniform sampler2D u_image;
layout (binding = 1) uniform sampler2D u_energy_map;

layout (std140, binding = 0) uniform DisplayParams {
	ivec2 u_window_size;
	ivec2 u_image_size;
	ivec2 u_texture_size;
	int u_debug_view_mode;
	int u_show_seam;
	int u_is_horizontal;
};

layout (std430, binding = 0) buffer SeamData {
	int u_seam_coords[];
};

float linear2srgb(float c) {
	return (c <= 0.0031308f) ? (c * 12.92f) : (1.055f * pow(c, 1.0f/2.4f) - 0.055f);
}
vec3 linear2srgb(vec3 c) {
	return vec3(linear2srgb(c.x), linear2srgb(c.y), linear2srgb(c.z));
}

void main() {
	const float window_aspect = float(u_window_size.x) / float(u_window_size.y);
	const float image_aspect = float(u_image_size.x) / float(u_image_size.y);

	vec2 scale = vec2(1.0f);
	vec2 offset = vec2(0.0f);

	if (window_aspect > image_aspect) /* Pillar-box */ {
		scale.x = image_aspect / window_aspect;
		offset.x = (1.0f - scale.x) * 0.5f;
	} else /* Letter-box */ {
		scale.y = window_aspect / image_aspect;
		offset.y = (1.0f - scale.y) * 0.5f;
	}

	const vec2 uv = (In.texcoord_0 - offset) / scale;

	if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f) {
		out_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return;
	}

	const vec2 final_uv = uv * (vec2(u_image_size) / vec2(u_texture_size));
	const ivec2 texel_coord = ivec2(uv * vec2(u_image_size));

	vec3 display_color = vec3(0.0f, 0.0f, 0.0f);

	if (u_debug_view_mode == 1 /* Energy */) {
		const float energy = texture(u_energy_map, final_uv).r;
		display_color = vec3(clamp(energy * 0.2f, 0.0f, 1.0f));
	} else /* None*/ {
		const vec3 linear_color = texture(u_image, final_uv).rgb;
		display_color = linear2srgb(linear_color);
	}

	if (bool(u_show_seam)) {
		bool is_seam = false;
		if (bool(u_is_horizontal)) {
			if (texel_coord.x < u_image_size.x) {
				const int seam_y = u_seam_coords[texel_coord.x];
				if (texel_coord.y == seam_y) {
					is_seam = true;
				}
			}
		} else {
			if (texel_coord.y < u_image_size.y) {
				const int seam_x = u_seam_coords[texel_coord.y];
				if (texel_coord.x == seam_x) {
					is_seam = true;
				}
			}
		}

		if (is_seam) {
			display_color = vec3(1.0f, 0.0f, 0.0f);
		}
	}

	out_color = vec4(display_color, 1.0f);
}
)");

	String8 const cs_srgb_to_linear = str8_literal(R"(
#version 460 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (binding = 0) uniform sampler2D u_image_srgb;
layout (rgba8, binding = 0) uniform image2D u_image_linear;

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

void main() {
	const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if (coord.x >= u_current_size.x || coord.y >= u_current_size.y) {
		return;
	}

	const vec4 linear_color = texelFetch(u_image_srgb, coord, 0); 
	imageStore(u_image_linear, coord, linear_color);
}
)");

	String8 const cs_sobel = str8_literal(R"(
#version 460 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (binding = 0) uniform sampler2D u_image;
layout (r32f, binding = 0) uniform image2D u_energy_map;

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

float luminance(vec3 c) {
	return dot(c, vec3(0.2126f, 0.7152f, 0.0722f));
}

void main() {
	const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if (coord.x >= u_current_size.x || coord.y >= u_current_size.y) {
		return;
	}

	const vec2 uv = (vec2(coord) + 0.5f) / vec2(u_texture_size); 
	const vec2 texel_size = 1.0f / vec2(u_texture_size);

	const float kernel_x[9] = float[9](
		-1.0f, 0.0f, 1.0f,
		-2.0f, 0.0f, 2.0f,
		-1.0f, 0.0f, 1.0f
	);
	const float kernel_y[9] = float[9](
		-1.0f, -2.0f, -1.0f,
		 0.0f,  0.0f,  0.0f,
		 1.0f,  2.0f,  1.0f
	);

	float gx = 0.0f;
	float gy = 0.0f;

	for (int y_offset = -1; y_offset <= 1; ++y_offset) {
		for (int x_offset = -1; x_offset <= 1; ++x_offset) {
			const int i = (y_offset + 1) * 3 + (x_offset + 1);
			const float lum = luminance(texture(u_image, uv + texel_size * vec2(x_offset, y_offset)).rgb);
			gx += kernel_x[i] * lum;
			gy += kernel_y[i] * lum;
		}
	}

	const float energy = abs(gx) + abs(gy);
	imageStore(u_energy_map, coord, vec4(energy));
}
)");

	String8 const cs_v_cost_row = str8_literal(R"(
#version 460 core
layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (binding = 1) uniform sampler2D u_energy_map;

layout (std430, binding = 0) buffer CostData {
	float u_cost_map[];
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

void main() {
	const int x = int(gl_GlobalInvocationID.x);
	if (x >= u_current_size.x) {
		return;
	}

	const int y = u_current_iteration;
	const int width = u_current_size.x;
	const int idx = y * width + x;
	const vec2 uv = (vec2(x, y) + 0.5f) / vec2(u_texture_size);
	const float energy = texture(u_energy_map, uv).r;

	if (y == 0) {
		u_cost_map[idx] = energy;
	} else {
		const int prev_row_idx = (y - 1) * width;
		const float C1 = u_cost_map[prev_row_idx + max(x - 1, 0)];
		const float C2 = u_cost_map[prev_row_idx + x];
		const float C3 = u_cost_map[prev_row_idx + min(x + 1, width - 1)];
		u_cost_map[idx] = energy + min(C1, min(C2, C3));
	}
}
)");

	String8 const cs_v_find_min_local = str8_literal(R"(
#version 460 core
layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 0) buffer CostData {
	float u_cost_map[];
};
layout (std430, binding = 2) buffer MinIndexData {
	uvec2 u_min_indices[]; // (cost_as_uint, index)
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

shared uvec2 s_min_data[256]; // (cost_as_uint, index)

void main() {
	const int x = int(gl_GlobalInvocationID.x);
	const int local_x = int(gl_LocalInvocationID.x);
	const int group_x = int(gl_WorkGroupID.x);
	const int width = u_current_size.x;

	float cost = 1e30f; // infinity
	if (x < width) {
		const int last_row_idx = (u_current_size.y - 1) * width;
		cost = u_cost_map[last_row_idx + x];
	}

	s_min_data[local_x] = uvec2(floatBitsToUint(cost), x);
	barrier();

	for (int s = 128; s > 0; s >>= 1) {
		if (local_x < s) {
			if (s_min_data[local_x + s].x < s_min_data[local_x].x) {
				s_min_data[local_x] = s_min_data[local_x + s];
			}
		}
		barrier();
	}

	if (local_x == 0) {
		u_min_indices[group_x] = s_min_data[0];
	}
}
)");

	String8 const cs_v_find_min_global = str8_literal(R"(
#version 460 core
layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 2) buffer MinIndexData {
	uvec2 u_min_indices[]; // (cost_as_uint, index)
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

shared uvec2 s_min_data[256];

void main() {
	const int local_x = int(gl_LocalInvocationID.x);
	const int group_count = int(ceil(float(u_current_size.x) / 256.0f));

	uvec2 min_val = uvec2(0xFFFFFFFF, 0);
	if (local_x < group_count) {
		min_val = u_min_indices[local_x];
	}
	s_min_data[local_x] = min_val;
	barrier();

	for (int s = 128; s > 0; s >>= 1) {
		if (local_x < s) {
			if (s_min_data[local_x + s].x < s_min_data[local_x].x) {
				s_min_data[local_x] = s_min_data[local_x + s];
			}
		}
		barrier();
	}

	if (local_x == 0) {
		u_min_indices[0] = s_min_data[0];
	}
}
)");

	String8 const cs_v_backtrace = str8_literal(R"(
#version 460 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 0) buffer CostData {
	float u_cost_map[];
};
layout (std430, binding = 1) buffer SeamData {
	int u_seam_coords[]; // x-coord for each row y
};
layout (std430, binding = 2) buffer MinIndexData {
	uvec2 u_min_indices[]; // (cost_as_uint, index)
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

void main() {
	const int y = u_current_iteration;
	const int width = u_current_size.x;

	if (y == u_current_size.y - 1) {
		u_seam_coords[y] = int(u_min_indices[0].y);
	} else {
		const int child_x = u_seam_coords[y + 1];

		int min_x = child_x;
		float min_cost = u_cost_map[y * width + min_x];

		if (child_x > 0) {
			float left_cost = u_cost_map[y * width + (child_x - 1)];
			if (left_cost < min_cost) {
				min_cost = left_cost;
				min_x = child_x - 1;
			}
		}

		if (child_x < width - 1) {
			float right_cost = u_cost_map[y * width + (child_x + 1)];
			if (right_cost < min_cost) {
				min_x = child_x + 1;
			}
		}

		u_seam_coords[y] = min_x;
	}
}
)");

	String8 const cs_v_remove_seam = str8_literal(R"(
#version 460 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (rgba8, binding = 0) uniform image2D u_image_in;
layout (rgba8, binding = 1) uniform image2D u_image_out;

layout (std430, binding = 0) buffer SeamData {
	int u_seam_coords[]; // x-coord for each row y
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

void main() {
	const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if (coord.x >= u_current_size.x - 1 || coord.y >= u_current_size.y) {
		return;
	}

	const int seam_x = u_seam_coords[coord.y];

	ivec2 read_coord = coord;
	if (coord.x >= seam_x) {
		read_coord.x += 1;
	}

	const vec4 color = imageLoad(u_image_in, read_coord);
	imageStore(u_image_out, coord, color);
}
)");

	String8 const cs_h_cost_col = str8_literal(R"(
#version 460 core
layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (binding = 1) uniform sampler2D u_energy_map;

layout (std430, binding = 0) buffer CostData {
	float u_cost_map[];
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

void main() {
	const int y = int(gl_GlobalInvocationID.x);
	if (y >= u_current_size.y) {
		return;
	}

	const int x = u_current_iteration;
	const int width = u_current_size.x;
	const int height = u_current_size.y;
	const int idx = y * width + x;
	const vec2 uv = (vec2(x, y) + 0.5f) / vec2(u_texture_size);
	const float energy = texture(u_energy_map, uv).r;

	if (x == 0) {
		u_cost_map[idx] = energy;
	} else {
		const int prev_col_idx = x - 1;
		const float C1 = u_cost_map[max(y - 1, 0) * width + prev_col_idx];
		const float C2 = u_cost_map[y * width + prev_col_idx];
		const float C3 = u_cost_map[min(y + 1, height - 1) * width + prev_col_idx];
		u_cost_map[idx] = energy + min(C1, min(C2, C3));
	}
}
)");

	String8 const cs_h_find_min_local = str8_literal(R"(
#version 460 core
layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 0) buffer CostData {
	float u_cost_map[];
};
layout (std430, binding = 2) buffer MinIndexData {
	uvec2 u_min_indices[]; // (cost_as_uint, index)
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

shared uvec2 s_min_data[256];

void main() {
	const int y = int(gl_GlobalInvocationID.x);
	const int local_y = int(gl_LocalInvocationID.x);
	const int group_y = int(gl_WorkGroupID.x);
	const int width = u_current_size.x;
	const int height = u_current_size.y;

	float cost = 1e30f;
	if (y < height) {
		const int last_col_idx = width - 1;
		cost = u_cost_map[y * width + last_col_idx];
	}

	s_min_data[local_y] = uvec2(floatBitsToUint(cost), y);
	barrier();

	for (int s = 128; s > 0; s >>= 1) {
		if (local_y < s) {
			if (s_min_data[local_y + s].x < s_min_data[local_y].x) {
				s_min_data[local_y] = s_min_data[local_y + s];
			}
		}
		barrier();
	}

	if (local_y == 0) {
		u_min_indices[group_y] = s_min_data[0];
	}
}
)");

	String8 const cs_h_find_min_global = str8_literal(R"(
#version 460 core
layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 2) buffer MinIndexData {
	uvec2 u_min_indices[]; // (cost_as_uint, index)
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

shared uvec2 s_min_data[256];

void main() {
	const int local_x = int(gl_LocalInvocationID.x);
	const int group_count = int(ceil(float(u_current_size.y) / 256.0f));

	uvec2 min_val = uvec2(0xFFFFFFFF, 0);
	if (local_x < group_count) {
		min_val = u_min_indices[local_x];
	}
	s_min_data[local_x] = min_val;
	barrier();

	for (int s = 128; s > 0; s >>= 1) {
		if (local_x < s) {
			if (s_min_data[local_x + s].x < s_min_data[local_x].x) {
				s_min_data[local_x] = s_min_data[local_x + s];
			}
		}
		barrier();
	}

	if (local_x == 0) {
		u_min_indices[0] = s_min_data[0];
	}
}
)");

	String8 const cs_h_backtrace = str8_literal(R"(
#version 460 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (std430, binding = 0) buffer CostData {
	float u_cost_map[];
};
layout (std430, binding = 1) buffer SeamData {
	int u_seam_coords[]; // y-coord for each col x
};
layout (std430, binding = 2) buffer MinIndexData {
	uvec2 u_min_indices[]; // (cost_as_uint, index)
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

void main() {
	const int x = u_current_iteration;
	const int width = u_current_size.x;
	const int height = u_current_size.y;

	if (x == width - 1) {
		u_seam_coords[x] = int(u_min_indices[0].y);
	} else {
		const int child_y = u_seam_coords[x + 1];

		int min_y = child_y;
		float min_cost = u_cost_map[min_y * width + x];

		if (child_y > 0) {
			float up_cost = u_cost_map[(child_y - 1) * width + x];
			if (up_cost < min_cost) {
				min_cost = up_cost;
				min_y = child_y - 1;
			}
		}

		if (child_y < height - 1) {
			float down_cost = u_cost_map[(child_y + 1) * width + x];
			if (down_cost < min_cost) {
				min_y = child_y + 1;
			}
		}

		u_seam_coords[x] = min_y;
	}
}
)");

	String8 const cs_h_remove_seam = str8_literal(R"(
#version 460 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (rgba8, binding = 0) uniform image2D u_image_in;
layout (rgba8, binding = 1) uniform image2D u_image_out;

layout (std430, binding = 0) buffer SeamData {
	int u_seam_coords[]; // y-coord for each col x
};

layout (std140, binding = 0) uniform CarveParams {
	ivec2 u_current_size;
	ivec2 u_texture_size;
	int u_current_iteration;
};

void main() {
	const ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if (coord.x >= u_current_size.x || coord.y >= u_current_size.y - 1) {
		return;
	}

	const int seam_y = u_seam_coords[coord.x];

	ivec2 read_coord = coord;
	if (coord.y >= seam_y) {
		read_coord.y += 1;
	}

	const vec4 color = imageLoad(u_image_in, read_coord);
	imageStore(u_image_out, coord, color);
}
)");
}
