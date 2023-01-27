/*****************************************************************//**
 * \file   quat.hpp
 * \brief  
 * 
 * \author hylu
 * \date   January 2023
 *********************************************************************/

#pragma once

// standard libraries
#include <array>
#include <iostream>

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>

#define QUATERNION_ACCESS(CHARACTER, INDEX)                                                                         \
    constexpr T &CHARACTER() { return e.at(INDEX); }

#define QUATERNION_ACCESS_CONST(CHARACTER, INDEX)                                                                   \
    constexpr const T &CHARACTER() const { return e.at(INDEX); }

namespace Horizon::math {

template<typename T = f32>
class Quaternion {
  public:
    constexpr Quaternion() noexcept {  }

    constexpr Quaternion(const T (&_e)[4]) noexcept {
        for (u32 i = 0; i < 4; i++) {
            e[i] = _e[i];
        }
    };

    constexpr Quaternion(T x, T y, T z, T w) noexcept {
        e[0] = x;
        e[1] = y;
        e[2] = z;
        e[3] = w;
    };

    ~Quaternion() noexcept {
    }

    constexpr T at(u32 i) const {
        assert(i <= 4);
        return e[i];
    }

    Quaternion(const Quaternion &rhs) noexcept = default;
    Quaternion &operator=(const Quaternion &rhs) noexcept = default;
    Quaternion(Quaternion &&rhs) noexcept = default;
    Quaternion &operator=(Quaternion &&rhs) noexcept = default;

    //// overload operators
    //constexpr auto operator+(const Quaternion<dimension, T> &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        ret(i) = at(i) + rhs.at(i);
    //    }
    //    return ret;
    //}

    //constexpr auto operator-(const Quaternion<dimension, T> &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        ret(i) = at(i) - rhs.at(i);
    //    }
    //    return ret;
    //}

    //constexpr auto operator*(const Quaternion<dimension, T> &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        ret(i) = at(i) * rhs.at(i);
    //    }
    //    return ret;
    //}

    //constexpr auto operator/(const Quaternion<dimension, T> &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        assert(rhs.at(i) != T(0)); // divide 0
    //        ret(i) = at(i) / rhs.at(i);
    //    }
    //    return ret;
    //}

    //constexpr auto &operator+=(const Quaternion<dimension, T> &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        (*this)(i) += rhs.at(i);
    //    }
    //    return *this;
    //}

    //constexpr auto &operator-=(const Quaternion<dimension, T> &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        (*this)(i) -= rhs.at(i);
    //    }
    //    return *this;
    //}

    //constexpr auto &operator*=(const Quaternion<dimension, T> &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        assert(rhs.at(i) != T(0)); // divide 0
    //        (*this)(i) *= rhs.at(i);
    //    }
    //    return *this;
    //}

    //constexpr auto &operator/=(const Quaternion<dimension, T> &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        (*this)(i) /= rhs.at(i);
    //    }
    //    return *this;
    //}
    //constexpr auto operator+(const T &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        ret(i) = at(i) + rhs;
    //    }
    //    return ret;
    //}

    //constexpr auto operator-(const T &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        ret(i) = at(i) - rhs;
    //    }
    //    return ret;
    //}

    //constexpr auto operator*(const T &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        ret(i) = at(i) * rhs;
    //    }
    //    return ret;
    //}

    //constexpr auto operator/(const T &rhs) {
    //    Quaternion<dimension, T> ret;
    //    for (u32 i = 0; i < dimension; i++) {
    //        assert(rhs != T(0)); // divide 0
    //        ret(i) = at(i) / rhs;
    //    }
    //    return ret;
    //}

    //constexpr auto &operator+=(const T &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        (*this)(i) += rhs;
    //    }
    //    return *this;
    //}

    //constexpr auto &operator-=(const T &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        (*this)(i) -= rhs;
    //    }
    //    return *this;
    //}

    //constexpr auto &operator*=(const T &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        assert(rhs.at(i) != T(0)); // divide 0
    //        (*this)(i) *= rhs;
    //    }
    //    return *this;
    //}

    //constexpr auto &operator/=(const T &rhs) {
    //    for (u32 i = 0; i < dimension; i++) {
    //        (*this)(i) /= rhs;
    //    }
    //    return *this;
    //}
    
    QUATERNION_ACCESS(x, 0)
    QUATERNION_ACCESS(y, 1)
    QUATERNION_ACCESS(z, 2)
    QUATERNION_ACCESS(w, 3)

    QUATERNION_ACCESS_CONST(x, 0)
    QUATERNION_ACCESS_CONST(y, 1)
    QUATERNION_ACCESS_CONST(z, 2)
    QUATERNION_ACCESS_CONST(w, 3)

  private:
    Vector<4, T> e{};
};

} // namespace Horizon::math