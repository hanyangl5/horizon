/*****************************************************************//**
 * \file   units.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/


#include "Units.h"

#include <runtime/core/math/Math.h>

namespace Horizon {

 f32 operator""_cm(long double x) {
     return static_cast<f32>(x / 100.0); 
 }

f32 operator""_m(long double x) {
    return static_cast<f32>(x); 
} // defult unit

f32 operator""_km(long double x) {
 return static_cast<f32>(x * 1000.0); 
}

 f32 operator""_lux(long double x) {
     return static_cast<f32>(x); 
 }

 f32 operator""_lm(long double x) {
     return static_cast<f32>(x * Math::_1DIVPI * 0.25f); 
 }

 f32 operator""_cd(long double x) {
     return static_cast<f32>(x); 
 }

 f32 operator""_rad(long double x) {
     return static_cast<f32>(x); 
 }

 f32 operator""_deg(long double x) {
     return static_cast<f32>(x * Math::_PI / 180.0f); 
 }

} // namespace Horizon