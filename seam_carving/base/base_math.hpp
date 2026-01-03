#pragma once

#include "base/base_types.hpp"

#define GLM_FORCE_INLINE
#define GLM_FORCE_NO_CTOR_INIT
#define GLM_FORCE_EXPLICIT_CTOR
#define GLM_FORCE_XYZW_ONLY // Incompatible with colors in HSV
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/integer.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/matrix_query.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/orthonormalize.hpp>
#include <glm/gtx/transform.hpp>

namespace dk {
	constexpr f32 CMP_EPSILON = 1e-5f;
	constexpr f32 CMP_EPSILON2 = CMP_EPSILON * CMP_EPSILON;

	constexpr f32 SQRT2 = glm::root_two<f32>();
	constexpr f32 SQRT3 = glm::root_three<f32>();
	constexpr f32 PI = glm::pi<f32>();
	constexpr f32 TAU = PI * 2.0f;
	constexpr f32 E = glm::e<f32>();
}

namespace dk {
	using glm::ivec2;
	using glm::ivec3;
	using glm::ivec4;
	using glm::uvec2;
	using glm::uvec3;
	using glm::uvec4;
	using glm::mat2;
	using glm::mat3;
	using glm::mat4;
	using glm::quat;
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
}
