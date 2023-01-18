/*****************************************************************//**
 * \file   units.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/


#include "Units.h"

// standard libraries

// third party libraries

// project headers
#include <runtime/core/math/math.h>

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
     return static_cast<f32>(x * math::_1DIVPI * 0.25f); 
 }

 f32 operator""_cd(long double x) {
     return static_cast<f32>(x); 
 }

 f32 operator""_rad(long double x) {
     return static_cast<f32>(x); 
 }

 f32 operator""_deg(long double x) {
     return static_cast<f32>(x * math::_PI / 180.0f); 
 }

} // namespace Horizon