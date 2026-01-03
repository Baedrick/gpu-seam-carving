#pragma once

#ifndef DK_ASSERT
#	ifdef _MSC_VER
#		ifndef NDEBUG
#			include <intrin.h>
#			define DK_ASSERT(x) do { if (!(x)) { __debugbreak(); } } while(false) /* NOLINT */
#		else
#			define DK_ASSERT(x) do { if (!(x)) { (void)(sizeof(x)); } } while(false) /* NOLINT */
#		endif
#	else
#		include <assert.h>
#		define DK_ASSERT(x) assert(x) /* NOLINT */
#	endif
#endif
