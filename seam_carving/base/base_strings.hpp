#pragma once

#include "base/base_arena.hpp"
#include "base/base_types.hpp"

#include "thirdparty/stb_sprintf.h"

namespace dk {
	struct String8 {
		u8 const *data;
		u64 size;
	};

	struct String16 {
		u16 const *data;
		u64 size;
	};

	struct String8Node {
		String8Node *next;
		String8 string;
	};

	struct String8List {
		String8Node *first;
		String8Node *last;
		u64 node_count;
		u64 total_size;
	};

	struct StringJoinParams {
		String8 prefix;
		String8 postfix;
		String8 separator;
	};

	enum StringMatchFlags : u8 {
		STRING_MATCH_FLAG_NONE = 0,
		STRING_MATCH_FLAG_CASE_INSENSITIVE = 1u << 0,
		STRING_MATCH_FLAG_SLASH_INSENSITIVE = 1u << 1
	};

	struct UnicodeDecode {
		u32 codepoint;
		u32 advance;
	};


	/* --- Char Functions --- */ 

	auto char_is_alpha(u8 c) noexcept -> b8;
	
	auto char_is_alpha_upper(u8 c) noexcept -> b8;
	
	auto char_is_alpha_lower(u8 c) noexcept -> b8;

	auto char_is_digit(u8 c) noexcept -> b8;

	auto char_is_symbol(u8 c) noexcept -> b8;

	auto char_is_slash(u8 c) noexcept -> b8;

	auto char_is_whitespace(u8 c) noexcept -> b8;

	auto char_to_upper(u8 c) noexcept -> u8;

	auto char_to_lower(u8 c) noexcept -> u8;
	
	auto char_to_forward_slash(u8 c) noexcept -> u8;


	/* --- C-String Measurement --- */

	auto cstring_length(char const *cstr) noexcept -> u64;


	/* --- String Constructors --- */

	inline auto str8(u8 *str, u64 size) noexcept -> String8 {
		return { .data = str, .size = size };
	}

	template <usize N>
	auto str8_literal(char const (&cstr)[N]) noexcept -> String8 {
		return { .data = reinterpret_cast<u8 const *>(cstr), .size = N - 1 };
	}


	/* --- String Functions --- */
	
	auto str8_compare(String8 s1, String8 s2, StringMatchFlags flags) noexcept -> s32;

	auto str8_copy(Arena *arena, String8 str) noexcept -> String8;

	auto str8fv(Arena *arena, char const *fmt, va_list args) noexcept -> String8;

	auto str8f(Arena *arena, char const *fmt, ...) noexcept -> String8;


	/* --- String Lists --- */

	auto str8_list_push_node(String8List *list, String8Node *node) noexcept -> void;

	auto str8_list_push_node_front(String8List *list, String8Node *node) noexcept -> void;
	
	auto str8_list_push(Arena *arena, String8List *list, String8 str) noexcept -> void;
	
	auto str8_list_pushf(Arena *arena, String8List *list, char const *fmt, ...) noexcept -> void;

	auto str8_list_push_front(Arena *arena, String8List *list, String8 str) noexcept -> void;

	auto str8_list_split(Arena *arena, String8 string, String8 const *splits, u64 split_count) noexcept -> String8List;

	auto str8_list_join(Arena *arena, String8List list, StringJoinParams const *optional_params) noexcept -> String8;

	
	/* --- Unicode Conversions --- */

	auto utf8_decode(u8 const *str, u64 max) noexcept -> UnicodeDecode;

	auto utf16_decode(u16 const *str, u64 max) noexcept -> UnicodeDecode;

	auto utf8_encode(u8 *out, u32 codepoint) noexcept -> u32;

	auto utf16_encode(u16 *out, u32 codepoint) noexcept -> u32;


	/* --- Unicode String Conversions --- */

	auto str8_from_16(Arena *arena, String16 str) noexcept -> String8;

	auto str16_from_8(Arena *arena, String8 str) noexcept -> String16;


	/* --- Path Helpers --- */

	auto path_normalize_from_str8(Arena *arena, String8 path) noexcept -> String8;
}
