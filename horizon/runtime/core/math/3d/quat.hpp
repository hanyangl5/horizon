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
    

    
    constexpr T &x() {
        static_assert(dimension >= 1);
        return e.at(0);
    }

    constexpr T &y() {
        static_assert(dimension >= 2);
        return e.at(1);
    }

    constexpr T &z() {
        static_assert(dimension >= 3);
        return e.at(2);
    }

    constexpr T &w() {
        static_assert(dimension >= 4);
        return e.at(3);
    }

    constexpr const T &x() const {
        static_assert(dimension >= 1);
        return e.at(0);
    }

    constexpr const T &y() const {
        static_assert(dimension >= 2);
        return e.at(1);
    }

    constexpr const T &z() const {
        static_assert(dimension >= 3);
        return e.at(2);
    }

    constexpr const T &w() const {
        static_assert(dimension >= 4);
        return e.at(3);
    }
  private:
    Vector<4, T> e{};
};

} // namespace Horizon::math