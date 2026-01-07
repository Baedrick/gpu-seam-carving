#pragma once

#include "base/base_assert.h"
#include "base/base_types.hpp"

#include <type_traits>

namespace dk {
	template <typename T>
	constexpr auto kilo_bytes(T bytes) noexcept -> T {
		static_assert(std::is_integral_v<T>);
		return bytes << 10;
	}

	template <typename T>
	constexpr auto mega_bytes(T bytes) noexcept -> T {
		static_assert(std::is_integral_v<T>);
		return bytes << 20;
	}

	template <typename T>
	constexpr auto giga_bytes(T bytes) noexcept -> T {
		static_assert(std::is_integral_v<T>);
		return bytes << 30;
	}

	template <typename T, usize N>
	constexpr auto array_size(T const (&)[N]) noexcept -> usize {
		return N;
	}

	template <typename T>
	auto swap(T *a, T *b) noexcept -> void {
		T const tmp = *a;
		*a = *b;
		*b = tmp;
	}

	template <typename T>
	constexpr auto is_pow_2(T x) noexcept -> b8 {
		static_assert(std::is_unsigned_v<T>);
		return x > 0 && (x & (x - 1)) == 0;
	}

	template <typename T>
	constexpr auto bitmask(u32 n) noexcept -> T {
		static_assert(std::is_unsigned_v<T>);
		return (n >= (sizeof(T) * 8 - 1)) ? static_cast<T>(-1) : (static_cast<T>(1) << (n + 1)) - 1;
	}

	template <typename T>
	constexpr auto bit(u32 n) noexcept -> T {
		static_assert(std::is_unsigned_v<T>);
		return (n >= (sizeof(T) * 8)) ? static_cast<T>(0) : (static_cast<T>(1) << n);
	}

	inline auto align_forward_pow_2(std::uintptr_t value, usize align) noexcept -> std::uintptr_t {
		DK_ASSERT(is_pow_2(align)); // 1, 2, 4, 8, 16, etc.
		return (value + align - 1) & ~(align - 1);
	}
}
