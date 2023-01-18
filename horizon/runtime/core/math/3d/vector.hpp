/*****************************************************************//**
 * \file   FILE_NAME
 * \brief  BRIEF
 * 
 * \author XXX
 * \date   XXX
 *********************************************************************/

#pragma once

// standard libraries
#include <array>
#include <iostream>
#include <cassert>

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>

namespace Horizon::math {

template<u32 dimension, typename T = f32>
class Vector {
  public:
    constexpr Vector() noexcept {
      static_assert(dimension <= MAX_DIMENSION);
    }

    constexpr Vector(const T(&_e)[dimension]) noexcept {
      static_assert(dimension <= MAX_DIMENSION);
      for (u32 i = 0; i < dimension;i++) {
        e[i] = _e[i];
      }
    };

    // Vector2
    constexpr Vector(T x, T y) noexcept {
      static_assert(dimension == 2);
      e[0] = x;
      e[1] = y;
    };

    // Vector3
    constexpr Vector(T x, T y, T z) noexcept {
      static_assert(dimension == 3);
      e[0] = x;
      e[1] = y;
      e[2] = z;
    };

    // constrcut Vector3 by [Vector2, z]
    constexpr Vector(Vector<2, T> xy, T z) noexcept {
      static_assert(dimension == 3);
      e[0] = xy.at(0);
      e[1] = xy.at(1);
      e[2] = z;
    };

    // Vector4
    constexpr Vector(T x, T y, T z, T w) noexcept {
      static_assert(dimension == 4);
      e[0] = x;
      e[1] = y;
      e[2] = z;
      e[3] = w;
    };

    // constrcut Vector4 by [vec2, y, z]
    constexpr Vector(Vector<2, T> xy, T z, T w) noexcept {
      static_assert(dimension == 4);
      e[0] = xy.at(0);
      e[1] = xy.at(1);
      e[2] = z;
      e[3] = w;
    };

    // constrcut Vector4 by [vec3, z]
    constexpr Vector(Vector<3, T> xyz, T w) noexcept {
      static_assert(dimension == 4);
      e[0] = xyz.at(0);
      e[1] = xyz.at(1);
      e[2] = xyz.at(2);
      e[3] = w;
    };

    ~Vector() noexcept {
      static_assert(dimension <= MAX_DIMENSION);
    }

    constexpr T at(u32 i) const {
      assert(i <= dimension);
      return e[i]; 
    }

    constexpr T& operator()(u32 i) {
      assert(i <= dimension);
      return e[i]; 
    }

    Vector(const Vector &rhs) noexcept = default;
    Vector &operator=(const Vector &rhs) noexcept = default;
    Vector(Vector &&rhs) noexcept = default;
    Vector &operator=(Vector &&rhs) noexcept = default;
        
    // overload operators
    constexpr auto operator+(const Vector<dimension, T>& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        ret(i) = at(i) + rhs.at(i);
      }
      return ret;
    }

    constexpr auto operator-(const Vector<dimension, T>& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        ret(i) = at(i) - rhs.at(i);
      }
      return ret;
    }

    constexpr auto operator*(const Vector<dimension, T>& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        ret(i) = at(i) * rhs.at(i);
      }
      return ret;
    }

    constexpr auto operator/(const Vector<dimension, T>& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        assert(rhs.at(i) != T(0)); // divide 0
        ret(i) = at(i) / rhs.at(i);
      }
      return ret;
    }

    constexpr auto& operator+=(const Vector<dimension, T>& rhs) {
      for (u32 i = 0; i < dimension;i++) {
        (*this)(i) += rhs.at(i);
      }
      return *this;
    }

    constexpr auto& operator-=(const Vector<dimension, T>& rhs) {
      for (u32 i = 0; i < dimension;i++) {
        (*this)(i) -= rhs.at(i);
      }
      return *this;
    }

    constexpr auto& operator*=(const Vector<dimension, T>& rhs) {
      for (u32 i = 0; i < dimension;i++) {
        assert(rhs.at(i) != T(0)); // divide 0
        (*this)(i) *= rhs.at(i);
      }
      return *this;
    }

    constexpr auto& operator/=(const Vector<dimension, T>& rhs) {
      for (u32 i = 0; i < dimension;i++) {
          (*this)(i) /= rhs.at(i);
      }
      return *this;
    }
    constexpr auto operator+(const T& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        ret(i) = at(i) + rhs;
      }
      return ret;
    }

    constexpr auto operator-(const T& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        ret(i) = at(i) - rhs;
      }
      return ret;
    }

    constexpr auto operator*(const T& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        ret(i) = at(i) * rhs;
      }
      return ret;
    }

    constexpr auto operator/(const T& rhs) {
      Vector<dimension, T> ret;
      for (u32 i = 0; i < dimension;i++) {
        assert(rhs != T(0)); // divide 0
        ret(i) = at(i) / rhs;
      }
      return ret;
    }

    constexpr auto& operator+=(const T& rhs) {
      for (u32 i = 0; i < dimension;i++) {
        (*this)(i) += rhs;
      }
      return *this;
    }

    constexpr auto& operator-=(const T& rhs) {
      for (u32 i = 0; i < dimension;i++) {
        (*this)(i) -= rhs;
      }
      return *this;
    }

    constexpr auto& operator*=(const T& rhs) {
      for (u32 i = 0; i < dimension;i++) {
        assert(rhs.at(i) != T(0)); // divide 0
        (*this)(i) *= rhs;
      }
      return *this;
    }

    constexpr auto& operator/=(const T& rhs) {
      for (u32 i = 0; i < dimension;i++) {
          (*this)(i) /= rhs;
      }
      return *this;
    }
private:
  std::array<T, dimension> e{};
};

}