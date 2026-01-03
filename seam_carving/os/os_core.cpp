#include "os_core.hpp"

auto dk::os_handle_invalid() noexcept -> OS_Handle {
	return { 0 };
}
