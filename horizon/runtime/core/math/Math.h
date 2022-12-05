/*****************************************************************//**
 * \file   Math.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <runtime/core/utils/Definations.h>

#include "CommonMath.h"
#include "3DMath.h"
#include "CEMath.h"

namespace Horizon::Math {


template <typename T, u32 size, u32 dimension = 1> auto UniformDistribution() {
    if constexpr (!std::is_integral<T>() && !std::is_floating_point<T>()) {
        static_assert(false);
    }
    std::uniform_real_distribution<T> rnd_dist(0.0, 1.0);
    std::default_random_engine generator;
    constexpr u32 array_size = CE::PowI(size, dimension);
    Container::FixedArray<T, array_size> sequence{};
    for (u32 i = 0; i < array_size; i++) {
        sequence[i] = rnd_dist(generator);
    }
    return sequence;
}



// TODO(hylu): provide math functions to replace std::xxx()

// random
//
//template <typename T, u32 size> Container::FixedArray<T, size> HaltonSequence() {
//    return Container::FixedArray<T, size>;
//}

} // namespace Horizon::Math