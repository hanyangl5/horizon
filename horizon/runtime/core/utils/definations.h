/*****************************************************************//**
 * \file   definations.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#define NOMINMAX

#include <cstdint>

#include <d3d12.h>
#include <vulkan/vulkan.h>

#include "../memory/Alloc.h"
#include "../container/Container.h"

namespace Horizon {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

#define USE_ASYNC_COMPUTE_TRANSFER 1
#define RENDERDOC_ENABLED 1

} // namespace Horizon