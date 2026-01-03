#pragma once

namespace dk {
	template <typename T>
	auto list_stack_push(T **first, T *node) noexcept -> void {
		node->next = *first;
		*first = node;
	}

	template <typename T>
	auto list_stack_pop(T **first) noexcept -> void {
		if (*first != nullptr) {
			*first = (*first)->next;
		}
	}

	template <typename T>
	auto list_queue_push(T **first, T **last, T *node) noexcept -> void {
		node->next = nullptr;
		if (*first == nullptr) {
			*first = *last = node;
		} else {
			(*last)->next = node;
			*last = node;
		}
	}

	template <typename T>
	auto list_queue_pop(T **first, T **last) noexcept -> void {
		if (*first == *last) {
			*first = *last = nullptr;
		} else {
			*first = (*first)->next;
		}
	}

	template <typename T>
	auto list_push_front(T **first, T **last, T *node) noexcept -> void {
		if (*first == nullptr) {
			*first = *last = node;
			node->next = nullptr;
			node->prev = nullptr;
		} else {
			node->next = *first;
			node->prev = nullptr;
			(*first)->prev = node;
			*first = node;
		}
	}

	template <typename T>
	auto list_push_back(T **first, T **last, T *node) noexcept -> void {
		if (*first == nullptr) {
			*first = *last = node;
			node->next = nullptr;
			node->prev = nullptr;
		} else {
			node->prev = *last;
			node->next = nullptr;
			(*last)->next = node;
			*last = node;
		}
	}

	template <typename T>
	auto list_pop_front(T **first, T **last) noexcept -> void {
		if (*first == nullptr) {
			return;
		}
		if (*first == *last) {
			*first = *last = nullptr;
		} else {
			*first = (*first)->next;
			(*first)->prev = nullptr;
		}
	}

	template <typename T>
	auto list_pop_back(T **first, T **last) noexcept -> void {
		if (*last == nullptr) {
			return;
		}
		if (*first == *last) {
			*first = *last = nullptr;
		} else {
			*last = (*last)->prev;
			(*last)->next = nullptr;
		}
	}

	template <typename T>
	auto list_remove(T **first, T **last, T *node) noexcept -> void {
		if (node->prev != nullptr) {
			node->prev->next = node->next;
		}
		if (node->next != nullptr) {
			node->next->prev = node->prev;
		}
		if (*first == node) {
			*first = node->next;
		}
		if (*last == node) {
			*last = node->prev;
		}
		node->next = nullptr;
		node->prev = nullptr;
	}
}
