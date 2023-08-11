/*****************************************************************/ /**
 * \file   CommonMath.h
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#pragma once

#include <runtime/core/utils/definations.h>

namespace Horizon::Math {

static constexpr f32 _PI = 3.141592654f;
static constexpr f32 _2PI = 6.283185307f;
static constexpr f32 _1DIVPI = 0.318309886f;
static constexpr f32 _1DIV2PI = 0.159154943f;
static constexpr f32 _PIDIV2 = 1.570796327f;
static constexpr f32 _PIDIV4 = 0.785398163f;

template <typename T> inline T Lerp(T a, T b, f32 t) { return a + t * (b - a); }

} // namespace Horizon::Math

namespace Horizon {

template <typename T> T AlignUp(T a, T b) { return (a + T(b - 1)) / b; }

} // namespace Horizon
