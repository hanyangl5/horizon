/*****************************************************************//**
 * \file   math.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>
#include <runtime/core/math/common.h>
#include <runtime/core/math/3d/3dmath.h>

namespace Horizon::math {

using Vector4f = Vector<4, f32>;
using Vector3f = Vector<3, f32>;
using Vector2f = Vector<2, f32>;

using Matrix44f = Matrix<4, 4, f32>;

using Quat = Quaternion<f32>;

extern Vector<3> DEFAULT_ORIENTATION;
}

// #include "cemath/cemath.h"
