// user defined literals for physical unit
#pragma once

#include <runtime/core/utils/definations.h>

namespace Horizon {

// length

constexpr f32 operator"" _cm(long double);

constexpr f32 operator"" _m(long double);

constexpr f32 operator"" _km(long double);

// luminousity

constexpr f32 operator"" _lux(long double);

constexpr f32 operator"" _lm(long double);

constexpr f32 operator"" _cd(long double);

// degree

constexpr f32 operator"" _rad(long double);

constexpr f32 operator"" _deg(long double);

} // namespace Horizon