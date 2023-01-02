/*****************************************************************//**
 * \file   functions.h
 * \brief  
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#pragma once

#include <runtime/core/utils/definations.h>

namespace Horizon {

template <typename T> inline T Lerp(T a, T b, f32 t) { return a + t * (b - a); }

template <typename T> T AlignUp(T a, T b) { return (a + T(b - 1)) / b; }

} // namespace Horizon