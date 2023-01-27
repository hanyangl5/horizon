/*****************************************************************/ /**
 * \file   circular.h
 * \brief  
 * 
 * \author hanyanglu
 * \date   January 2023
 *********************************************************************/

#pragma once

// standard libraries
#include <cmath>

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>

namespace Horizon::math {

inline f32 Sin(f32 x) { return std::sin(x); }

inline f32 Cos(f32 x) { return std::cos(x); }

inline f32 Tan(f32 x) { return std::tan(x); }

inline f32 ArcSin(f32 x) { return std::asin(x); }

inline f32 ArcCos(f32 x) { return std::acos(x); }

inline f32 ArcTan(f32 x) { return std::atan(x); }

} // namespace Horizon::math
