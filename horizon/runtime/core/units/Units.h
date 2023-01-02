/*****************************************************************//**
 * \file   Units.h
 * \brief  user defined literals for physical unit
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <runtime/core/utils/definations.h>

namespace Horizon {

// length

 f32 operator"" _cm(long double);

 f32 operator"" _m(long double);

 f32 operator"" _km(long double);

// luminousity

 f32 operator"" _lux(long double);

 f32 operator"" _lm(long double);

 f32 operator"" _cd(long double);

// degree

 f32 operator"" _rad(long double);

 f32 operator"" _deg(long double);

 } // namespace Horizon