#include "base/base_assert.h"
#include <stdbool.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

#define STBI_ASSERT(x) DK_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBIW_ASSERT(x) DK_ASSERT(x)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
