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

// third party libraries

// project headers
#include <runtime/core/utils/definations.h>
#include <runtime/core/platform/platform.h>
#include <runtime/core/math/3d/vector.hpp>

namespace Horizon::math {

template<u32 dimension_m, u32 dimension_n, typename T = f32>
class Matrix {
  public:

    constexpr Matrix() noexcept {
        static_assert(dimension_m <= MAX_DIMENSION && dimension_n <= MAX_DIMENSION);
        if constexpr (dimension_m == dimension_n) {
            SetIdentity();
        }
    }

    constexpr Matrix(const T (&_e)[dimension_m * dimension_n]) noexcept {
        static_assert(dimension_m <= MAX_DIMENSION && dimension_n <= MAX_DIMENSION);
        // SIMD for 4x4 matrix
        if constexpr (PLATFORM_SUPPORT_SSE3_INTRINSICS && (dimension_m <= MAX_DIMENSION) &&
                      (dimension_n <= MAX_DIMENSION)) {
        
        } else {
            for (u32 i = 0; i < dimension_m; i++) {
                for (u32 j = 0; j < dimension_n; j++) {
                    e[i * dimension_n + j] = _e[i * dimension_n + j];
                }
            }
        }
    }

    constexpr Matrix(const Vector<3, T> &x, const Vector<3, T> &y, const Vector<3, T> &z) noexcept {
        static_assert((dimension_m == dimension_n) && ((dimension_m == 4) || (dimension_m == 3)));
        // SIMD for 4x4 matrix
        if constexpr (PLATFORM_SUPPORT_SSE3_INTRINSICS && (dimension_m <= MAX_DIMENSION) &&
                      (dimension_n <= MAX_DIMENSION)) {
        } else {
            for (u32 j = 0; j < 3; j++) {
                e[0 * dimension_n + j] = x.at(j);
            }
            for (u32 j = 0; j < 3; j++) {
                e[1 * dimension_n + j] = y.at(j);
            }

            for (u32 j = 0; j < 3; j++) {
                e[2 * dimension_n + j] = z.at(j);
            }
        }
    }
    ~Matrix() noexcept {
    }

    constexpr const T& at(u32 i) const {
      assert(i <= dimension_m * dimension_n);
      return e[i]; 
    }

    constexpr const T& at(u32 row, u32 column) const {
      assert(row <= dimension_m && column <= dimension_n);
      return this->at(row * dimension_n + column); 
    }
    
    constexpr T& at(u32 i) {
      assert(i <= dimension_m * dimension_n);
      return e[i];
    }

    constexpr T& at(u32 row, u32 column) {
      assert(row <= dimension_m && column <= dimension_n);
      return this->at(row * dimension_n + column);
    }

    // overload operators
    constexpr auto operator+(const Matrix<dimension_m, dimension_n, T>& rhs) {

    }

    Matrix(const Matrix &rhs) noexcept = default;
    Matrix &operator=(const Matrix &rhs) noexcept = default;
    Matrix(Matrix &&rhs) noexcept = default;
    Matrix &operator=(Matrix &&rhs) noexcept = default;

    void SetIdentity() noexcept { 
        if constexpr (dimension_m != dimension_n) {
            // TODO(hylu): warning msg
            return;
        }
        for (u32 i = 0; i < dimension_m;i++) {
            this->at(i, i) = T(1);
        }
    }

  private:
    std::array<T, dimension_m * dimension_n> e{}; // row major
};

}