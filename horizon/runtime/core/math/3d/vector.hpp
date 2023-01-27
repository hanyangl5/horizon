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

#define VECTOR_ACCESS(CHARACTER, INDEX, LENGTH)                                                                       \
    constexpr T &CHARACTER() {                                                                                         \
        static_assert(dimension >= LENGTH);                                                                       \
        return at(INDEX);                                                                                              \
    }

#define VECTOR_ACCESS_CONST(CHARACTER, INDEX, LENGTH)                                                                        \
    constexpr const T &CHARACTER() const {                                                                                         \
        static_assert(dimension >= LENGTH);                                                                            \
        return at(INDEX);                                                                                              \
    }

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
    constexpr Vector(const T &x, const T &y) noexcept {
      static_assert(dimension == 2);
      e[0] = x;
      e[1] = y;
    };

    // Vector3
    constexpr Vector(const T &x, const T &y, const T &z) noexcept {
      static_assert(dimension == 3);
      e[0] = x;
      e[1] = y;
      e[2] = z;
    };

    // constrcut Vector3 by [Vector2, z]
    constexpr Vector(const Vector<2, T> &xy, const T &z) noexcept {
      static_assert(dimension == 3);
      e[0] = xy.at(0);
      e[1] = xy.at(1);
      e[2] = z;
    };

    // Vector4
    constexpr Vector(const T &x, const T &y, const T &z, const T &w) noexcept {
      static_assert(dimension == 4);
      e[0] = x;
      e[1] = y;
      e[2] = z;
      e[3] = w;
    };

    // constrcut Vector4 by [vec2, y, z]
    constexpr Vector(const Vector<2, T> &xy, const T &z, const T &w) noexcept {
      static_assert(dimension == 4);
      e[0] = xy.at(0);
      e[1] = xy.at(1);
      e[2] = z;
      e[3] = w;
    };

    // constrcut Vector4 by [vec3, z]
    constexpr Vector(const Vector<3, T>& xyz, const T &w) noexcept {
      static_assert(dimension == 4);
      e[0] = xyz.at(0);
      e[1] = xyz.at(1);
      e[2] = xyz.at(2);
      e[3] = w;
    };

    constexpr Vector(const Vector &rhs) noexcept = default;
    constexpr Vector &operator=(const Vector &rhs) noexcept = default;
    constexpr Vector(Vector &&rhs) noexcept = default;
    constexpr Vector &operator=(Vector &&rhs) noexcept = default;

    ~Vector() noexcept {
      static_assert(dimension <= MAX_DIMENSION);
    }

    constexpr const T& at(u32 i) const {
      assert(i <= dimension);
      return e[i]; 
    }

    constexpr T& at(u32 i) {
      assert(i <= dimension);
      return e[i]; 
    }

    VECTOR_ACCESS(x, 0, 1)
    VECTOR_ACCESS(y, 1, 2)
    VECTOR_ACCESS(z, 2, 3)
    VECTOR_ACCESS(w, 3, 4)
    VECTOR_ACCESS(r, 0, 1)
    VECTOR_ACCESS(g, 1, 2)
    VECTOR_ACCESS(b, 2, 3)
    VECTOR_ACCESS(a, 3, 4)
   
    VECTOR_ACCESS_CONST(x, 0, 1)
    VECTOR_ACCESS_CONST(y, 1, 2)
    VECTOR_ACCESS_CONST(z, 2, 3)
    VECTOR_ACCESS_CONST(w, 3, 4)
    VECTOR_ACCESS_CONST(r, 0, 1)
    VECTOR_ACCESS_CONST(g, 1, 2)
    VECTOR_ACCESS_CONST(b, 2, 3)
    VECTOR_ACCESS_CONST(a, 3, 4)

  private:
    std::array<T, dimension> e{};
};

}