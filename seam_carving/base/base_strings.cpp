#include "base_strings.hpp"
#include "base_utils.hpp"

auto dk::char_is_alpha(u8 c) noexcept -> b8 {
	return char_is_alpha_upper(c) || char_is_alpha_lower(c);
}

auto dk::char_is_alpha_upper(u8 c) noexcept -> b8 {
	return c >= 'A' && c <= 'Z';
}

auto dk::char_is_alpha_lower(u8 c) noexcept -> b8 {
	return c >= 'a' && c <= 'z';
}

auto dk::char_is_digit(u8 c) noexcept -> b8 {
	return c >= '0' && c <= '9';
}

auto dk::char_is_symbol(u8 c) noexcept -> b8 {
	return c == '~' || c == '!' || c == '$' || c == '%' || c == '^'
		|| c == '&' || c == '*' || c == '-' || c == '=' || c == '+'
		|| c == '<' || c == '.' || c == '>' || c == '/' || c == '?'
		|| c == '|' || c == '\\'|| c == '{' || c == '}' || c == '('
		|| c == ')' || c == '[' || c == ']' || c == '#' || c == ','
		|| c == ';' || c == ':' || c == '@';
}

auto dk::char_is_slash(u8 c) noexcept -> b8 {
	return c == '/';
}

auto dk::char_is_whitespace(u8 c) noexcept -> b8 {
	return c == ' ' || c == '\r' || c == '\t' || c == '\f' || c == '\v' || c == '\n';
}

auto dk::char_to_upper(u8 c) noexcept -> u8 {
	return c >= 'a' && c <= 'z' ? 'A' + (c - 'a') : c;
}

auto dk::char_to_lower(u8 c) noexcept -> u8 {
	return c >= 'A' && c <= 'Z' ? 'a' + (c - 'A') : c;
}

auto dk::char_to_forward_slash(u8 c) noexcept -> u8 {
	return c == '\\' ? '/' : c;
}

auto dk::cstring_length(char const *cstr) noexcept -> u64 {
	u64 length = 0;
	for (; cstr[length]; length += 1) { } 
	return length;
}

auto dk::str8_compare(String8 s1, String8 s2, StringMatchFlags flags) noexcept -> s32 {
	u64 const min_size = s1.size < s2.size ? s1.size : s2.size;
	for (u64 i = 0; i < min_size; ++i) {
		u8 c1 = s1.data[i];
		u8 c2 = s2.data[i];
		if ((flags & STRING_MATCH_FLAG_CASE_INSENSITIVE) != 0) {
			c1 = char_to_lower(c1);
			c2 = char_to_lower(c2);
		}
		if ((flags & STRING_MATCH_FLAG_SLASH_INSENSITIVE) != 0) {
			c1 = char_to_forward_slash(c1);
			c2 = char_to_forward_slash(c2);
		}
		if (c1 != c2) {
			return static_cast<s32>(c1) - static_cast<s32>(c2);
		}
	}

	if (s1.size < s2.size) {
		return -1;
	}
	if (s1.size > s2.size) {
		return 1;
	}
	return 0;
}

auto dk::str8_copy(Arena *arena, String8 str) noexcept -> String8 {
	u8 *arr = arena_push_type_array<u8>(arena, str.size + 1);
	std::memcpy(arr, str.data, str.size);
	arr[str.size] = '\0';
	return { .data = arr, .size = str.size };
}

auto dk::str8fv(Arena *arena, char const *fmt, va_list args) noexcept -> String8 {
	va_list args2;
	va_copy(args2, args);
	s32 const needed_bytes = stbsp_vsnprintf(nullptr, 0, fmt, args) + 1; // NOLINT(clang-diagnostic-format-nonliteral)
	u8 *str = arena_push_type_array<u8>(arena, needed_bytes);
	stbsp_vsnprintf(reinterpret_cast<char *>(str), needed_bytes, fmt, args2); // NOLINT(clang-diagnostic-format-nonliteral)
	return { .data = str, .size = static_cast<u64>(needed_bytes - 1) };
}

auto dk::str8f(Arena *arena, char const *fmt, ...) noexcept -> String8 {
	va_list args;
	va_start(args, fmt);
	String8 const result = str8fv(arena, fmt, args);
	va_end(args);
	return result;
}

auto dk::str8_list_push_node(String8List *list, String8Node *node) noexcept -> void {
	node->next = nullptr;
	if (list->last) {
		list->last->next = node;
		list->last = node;
	} else {
		list->first = list->last = node;
	}
	list->node_count += 1;
	list->total_size += node->string.size;
}

auto dk::str8_list_push_node_front(String8List *list, String8Node *node) noexcept -> void {
	node->next = list->first;
	list->first = node;
	if (list->last == nullptr) {
		list->last = node;
	}
	list->node_count += 1;
	list->total_size += node->string.size;
}

auto dk::str8_list_push(Arena *arena, String8List *list, String8 str) noexcept -> void {
	String8Node *node = arena_push_type<String8Node>(arena);
	node->string = str;
	str8_list_push_node(list, node);
}

auto dk::str8_list_pushf(Arena *arena, String8List *list, char const *fmt, ...) noexcept -> void {
	va_list args;
	va_start(args, fmt);
	String8 str = str8fv(arena, fmt, args);
	va_end(args);
	str8_list_push(arena, list, str);
}

auto dk::str8_list_push_front(Arena *arena, String8List *list, String8 str) noexcept -> void {
	String8Node *node = arena_push_type<String8Node>(arena);
	node->string = str;
	str8_list_push_node_front(list, node);
}

auto dk::str8_list_split(Arena *arena, String8 string, String8 const *splits, u64 split_count) noexcept -> String8List {
	String8List list = {};
	u64 split_start = 0;

	for (u64 i = 0; i < string.size; i += 1) {
		b8 was_split = false;

		for (u64 s = 0; s < split_count; s += 1) {
			String8 const split = splits[s];
			if (i + split.size <= string.size) {
				if (std::memcmp(string.data + i, split.data, split.size) == 0) {
					String8 const segment = { .data = string.data + split_start, .size = i - split_start };
					str8_list_push(arena, &list, segment);
					split_start = i + split.size;
					// NOTE(Dedrick): Skip the delimiter (-1 because the loop increment adds 1).
					i += split.size - 1;
					was_split = true;
					break;
				}
			}
		}

		if (!was_split && i == string.size - 1) {
			String8 const segment = { .data = string.data + split_start, .size = i + 1 - split_start };
			str8_list_push(arena, &list, segment);
			break;
		}
	}

	return list;
}

auto dk::str8_list_join(Arena *arena, String8List list, StringJoinParams const *optional_params) noexcept -> String8 {
	StringJoinParams params = {};
	if (optional_params) {
		params = *optional_params;
	}

	u64 total_size = params.prefix.size + params.postfix.size + list.total_size;
	if (list.node_count > 1) {
		total_size += params.separator.size * (list.node_count - 1);
	}

	u8 *data = arena_push_type_array<u8>(arena, total_size + 1);
	u8 *ptr = data;

	std::memcpy(ptr, params.prefix.data, params.prefix.size);
	ptr += params.prefix.size;

	b8 first = true;
	for (String8Node const *node = list.first; node; node = node->next) {
		if (!first) {
			std::memcpy(ptr, params.separator.data, params.separator.size);
			ptr += params.separator.size;
		}
		std::memcpy(ptr, node->string.data, node->string.size);
		ptr += node->string.size;
		first = false;
	}

	std::memcpy(ptr, params.postfix.data, params.postfix.size);
	ptr += params.postfix.size;

	*ptr = '\0';

	return { .data = data, .size = total_size };
}

namespace {
	constexpr dk::u8 utf8_class[32] = {
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,2,2,2,2,3,3,4,5,
	};
}

// https://github.com/EpicGamesExt/raddebugger/blob/master/src/base/base_strings.c#L1836
auto dk::utf8_decode(u8 const *str, u64 max) noexcept -> UnicodeDecode {
	UnicodeDecode result = { .codepoint = static_cast<u32>(-1), .advance = 1 };
	u8 const byte = str[0];
	u8 const byte_class = utf8_class[byte >> 3];
	switch (byte_class) {
		case 1: {
			result.codepoint = byte;
			break;
		}
		case 2: {
			if(2 <= max) {
				u8 const cont_byte = str[1];
				if(utf8_class[cont_byte >> 3] == 0) {
					result.codepoint = (byte & bitmask<u32>(4)) << 6;
					result.codepoint |= cont_byte & bitmask<u32>(5);
					result.advance = 2;
				}
			}
			break;
		}
		case 3: {
			if(3 <= max) {
				u8 const cont_byte[2] = { str[1], str[2] };
				if (utf8_class[cont_byte[0] >> 3] == 0 &&
					utf8_class[cont_byte[1] >> 3] == 0) {
					result.codepoint = (byte & bitmask<u32>(3)) << 12;
					result.codepoint |= (cont_byte[0] & bitmask<u32>(5)) << 6;
					result.codepoint |= cont_byte[1] & bitmask<u32>(5);
					result.advance = 3;
				}
			}
			break;
		}
		case 4: {
			if(4 <= max) {
				u8 const cont_byte[3] = { str[1], str[2], str[3] };
				if (utf8_class[cont_byte[0] >> 3] == 0 &&
					utf8_class[cont_byte[1] >> 3] == 0 &&
					utf8_class[cont_byte[2] >> 3] == 0) {
					result.codepoint = (byte & bitmask<u32>(2)) << 18;
					result.codepoint |= ((cont_byte[0] & bitmask<u32>(5)) << 12);
					result.codepoint |= ((cont_byte[1] & bitmask<u32>(5)) <<  6);
					result.codepoint |=  (cont_byte[2] & bitmask<u32>(5));
					result.advance = 4;
				}
			}
			break;
		}
	}
	return result;
}

// https://github.com/EpicGamesExt/raddebugger/blob/master/src/base/base_strings.c#L1897
auto dk::utf16_decode(u16 const *str, u64 max) noexcept -> UnicodeDecode {
	UnicodeDecode result = {};
	result.codepoint = str[0];
	result.advance = 1;
	if (1 < max && 0xD800 <= str[0] && str[0] < 0xDC00 && 0xDC00 <= str[1] && str[1] < 0xE000) {
		result.codepoint = ((str[0] - 0xD800) << 10) | (str[1] - 0xDC00) + 0x10000;
		result.advance = 2;
	}
	return result;
}

// https://github.com/EpicGamesExt/raddebugger/blob/master/src/base/base_strings.c#L1911
auto dk::utf8_encode(u8 *out, u32 codepoint) noexcept -> u32 {
	u32 advance = 0;
	if (codepoint <= 0x7F) {
		out[0] = static_cast<u8>(codepoint);
		advance = 1;
	}
	else if (codepoint <= 0x7FF) {
		out[0] = static_cast<u8>((bitmask<u32>(1) << 6) | ((codepoint >> 6) & bitmask<u32>(4)));
		out[1] = static_cast<u8>( bit<u32>(7) | (codepoint & bitmask<u32>(5)));
		advance = 2;
	}
	else if (codepoint <= 0xFFFF) {
		out[0] = static_cast<u8>((bitmask<u32>(2) << 5) | ((codepoint >> 12) & bitmask<u32>(3)));
		out[1] = static_cast<u8>( bit<u32>(7) | ((codepoint >> 6) & bitmask<u32>(5)));
		out[2] = static_cast<u8>( bit<u32>(7) | ( codepoint       & bitmask<u32>(5)));
		advance = 3;
	}
	else if (codepoint <= 0x10FFFF) {
		out[0] = static_cast<u8>((bitmask<u32>(3) << 4) | ((codepoint >> 18) & bitmask<u32>(2)));
		out[1] = static_cast<u8>( bit<u32>(7) | ((codepoint >> 12) & bitmask<u32>(5)));
		out[2] = static_cast<u8>( bit<u32>(7) | ((codepoint >>  6) & bitmask<u32>(5)));
		out[3] = static_cast<u8>( bit<u32>(7) | ( codepoint        & bitmask<u32>(5)));
		advance = 4;
	}
	else {
		out[0] = '?';
		advance = 1;
	}
	return advance;
}

// https://github.com/EpicGamesExt/raddebugger/blob/master/src/base/base_strings.c#L1949
auto dk::utf16_encode(u16 *out, u32 codepoint) noexcept -> u32 {
	u32 advance = 1;
	if (codepoint == static_cast<u32>(-1)) {
		out[0] = static_cast<u16>('?');
	}
	else if (codepoint < 0x10000) {
		out[0] = static_cast<u16>(codepoint);
	}
	else {
		u64 const v = codepoint - 0x10000;
		out[0] = static_cast<u16>(0xD800 + (v >> 10));
		out[1] = static_cast<u16>(0xDC00 + (v & bitmask<u32>(9)));
		advance = 2;
	}
	return advance;
}

auto dk::str8_from_16(Arena *arena, String16 str) noexcept -> String8 {
	u64 const utf8_max_capacity = str.size * 3 + 1;
	u32 utf8_size = 0;
	u8 *utf8_ptr = arena_push_type_array<u8>(arena, utf8_max_capacity);
	
	u16 const *utf16_ptr = str.data;
	u16 const *const utf16_ptr_end = utf16_ptr + str.size;
	
	while (utf16_ptr < utf16_ptr_end) {
		UnicodeDecode const decoded = utf16_decode(
			utf16_ptr, 
			utf16_ptr_end - utf16_ptr
		);
		utf16_ptr += decoded.advance;
		utf8_size += utf8_encode(
			utf8_ptr + utf8_size, 
			decoded.codepoint
		);
	}

	utf8_ptr[utf8_size] = '\0';
	arena_pop(arena, utf8_max_capacity - (utf8_size + 1));
	return { .data = utf8_ptr, .size = utf8_size };
}

auto dk::str16_from_8(Arena *arena, String8 str) noexcept -> String16 {
	u64 const utf16_max_capacity = str.size * 2 + 1;
	u32 utf16_size = 0;
	u16 *utf16_ptr = arena_push_type_array<u16>(arena, utf16_max_capacity);

	u8 const *utf8_ptr = str.data;
	u8 const *const utf8_ptr_end = utf8_ptr + str.size;

	while (utf8_ptr < utf8_ptr_end) {
		UnicodeDecode const decoded = utf8_decode(
			utf8_ptr,
			utf8_ptr_end - utf8_ptr
		);
		utf8_ptr += decoded.advance;
		utf16_size += utf16_encode(
			utf16_ptr + utf16_size,
			decoded.codepoint
		);
	}

	utf16_ptr[utf16_size] = 0;
	arena_pop(arena, (utf16_max_capacity - (utf16_size + 1)) * sizeof(u16));

	return { .data = utf16_ptr, .size = utf16_size };
}

auto dk::path_normalize_from_str8(Arena *arena, String8 path) noexcept -> String8 {
	String8 const copy = str8_copy(arena, path);
	u8 *const data = const_cast<u8 *>(copy.data);
	for (u64 i = 0; i < copy.size; ++i) {
		data[i] = char_to_forward_slash(data[i]);
	}
	return copy;
}
