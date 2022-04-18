#pragma once

#define NOMINMAX
// stl

#include <runtime/core/math/Math.h>


namespace Horizon {
	struct RenderContext {
		u32 width;
		u32 height;
		u32 swap_chain_image_count = 3;
	};

}