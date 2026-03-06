#include "h3d_math.h"

#include <math.h>
#include <string.h>

#ifndef H3D_MATH_ENABLE_SIMD
#define H3D_MATH_ENABLE_SIMD 1
#endif

#ifndef H3D_MATH_ENABLE_SSE2
#define H3D_MATH_ENABLE_SSE2 1
#endif

#ifndef H3D_MATH_ENABLE_AVX
#define H3D_MATH_ENABLE_AVX 0
#endif

#ifndef H3D_MATH_ENABLE_NEON
#define H3D_MATH_ENABLE_NEON 1
#endif

#if H3D_MATH_ENABLE_SIMD && H3D_MATH_ENABLE_AVX && defined(__AVX__) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86))
#define H3D_MATH_SIMD_AVX 1
#include <immintrin.h>
#elif H3D_MATH_ENABLE_SIMD && H3D_MATH_ENABLE_SSE2 && \
    (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define H3D_MATH_SIMD_SSE2 1
#include <emmintrin.h>
#if defined(__SSE3__)
#include <pmmintrin.h>
#endif
#elif H3D_MATH_ENABLE_SIMD && H3D_MATH_ENABLE_NEON && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#define H3D_MATH_SIMD_NEON 1
#include <arm_neon.h>
#endif

#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
static inline __m128 h3d_load_vec3_x86(h3d_vec3 v)
{
    return _mm_set_ps(0.0f, v.z, v.y, v.x);
}

static inline h3d_vec3 h3d_store_vec3_x86(__m128 v)
{
    float out_data[4];
    _mm_storeu_ps(out_data, v);
    h3d_vec3 out = {out_data[0], out_data[1], out_data[2]};
    return out;
}

static inline float h3d_vec3_dot_x86(__m128 lhs, __m128 rhs)
{
    __m128 mul = _mm_mul_ps(lhs, rhs);
#if defined(__SSE3__)
    __m128 sum = _mm_hadd_ps(mul, mul);
    sum = _mm_hadd_ps(sum, sum);
    return _mm_cvtss_f32(sum);
#else
    float out_data[4];
    _mm_storeu_ps(out_data, mul);
    return out_data[0] + out_data[1] + out_data[2];
#endif
}

static inline __m128 h3d_vec3_cross_x86(__m128 lhs, __m128 rhs)
{
    const __m128 lhs_yzx = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128 rhs_zxy = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(3, 1, 0, 2));
    const __m128 lhs_zxy = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(3, 1, 0, 2));
    const __m128 rhs_yzx = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(3, 0, 2, 1));
    return _mm_sub_ps(_mm_mul_ps(lhs_yzx, rhs_zxy), _mm_mul_ps(lhs_zxy, rhs_yzx));
}

static h3d_mat4 h3d_mat4_mul_sse2(h3d_mat4 lhs, h3d_mat4 rhs)
{
    const __m128 rhs_row0 = _mm_loadu_ps(rhs.m[0]);
    const __m128 rhs_row1 = _mm_loadu_ps(rhs.m[1]);
    const __m128 rhs_row2 = _mm_loadu_ps(rhs.m[2]);
    const __m128 rhs_row3 = _mm_loadu_ps(rhs.m[3]);

    h3d_mat4 out;
    for (int i = 0; i < 4; ++i)
    {
        __m128 row = _mm_mul_ps(_mm_set1_ps(lhs.m[i][0]), rhs_row0);
        row = _mm_add_ps(row, _mm_mul_ps(_mm_set1_ps(lhs.m[i][1]), rhs_row1));
        row = _mm_add_ps(row, _mm_mul_ps(_mm_set1_ps(lhs.m[i][2]), rhs_row2));
        row = _mm_add_ps(row, _mm_mul_ps(_mm_set1_ps(lhs.m[i][3]), rhs_row3));
        _mm_storeu_ps(out.m[i], row);
    }
    return out;
}

static h3d_mat4 h3d_mat4_transpose_sse2(h3d_mat4 m)
{
    __m128 row0 = _mm_loadu_ps(m.m[0]);
    __m128 row1 = _mm_loadu_ps(m.m[1]);
    __m128 row2 = _mm_loadu_ps(m.m[2]);
    __m128 row3 = _mm_loadu_ps(m.m[3]);

    _MM_TRANSPOSE4_PS(row0, row1, row2, row3);

    h3d_mat4 out;
    _mm_storeu_ps(out.m[0], row0);
    _mm_storeu_ps(out.m[1], row1);
    _mm_storeu_ps(out.m[2], row2);
    _mm_storeu_ps(out.m[3], row3);
    return out;
}
#endif

#if defined(H3D_MATH_SIMD_AVX)
static h3d_mat4 h3d_mat4_mul_avx(h3d_mat4 lhs, h3d_mat4 rhs)
{
    const __m128 rhs_row0 = _mm_loadu_ps(rhs.m[0]);
    const __m128 rhs_row1 = _mm_loadu_ps(rhs.m[1]);
    const __m128 rhs_row2 = _mm_loadu_ps(rhs.m[2]);
    const __m128 rhs_row3 = _mm_loadu_ps(rhs.m[3]);

    __m256 rhs_row01 = _mm256_castps128_ps256(rhs_row0);
    rhs_row01 = _mm256_insertf128_ps(rhs_row01, rhs_row1, 1);

    __m256 rhs_row23 = _mm256_castps128_ps256(rhs_row2);
    rhs_row23 = _mm256_insertf128_ps(rhs_row23, rhs_row3, 1);

    h3d_mat4 out;
    for (int i = 0; i < 4; ++i)
    {
        __m256 weight01 = _mm256_castps128_ps256(_mm_set1_ps(lhs.m[i][0]));
        weight01 = _mm256_insertf128_ps(weight01, _mm_set1_ps(lhs.m[i][1]), 1);

        __m256 weight23 = _mm256_castps128_ps256(_mm_set1_ps(lhs.m[i][2]));
        weight23 = _mm256_insertf128_ps(weight23, _mm_set1_ps(lhs.m[i][3]), 1);

        const __m256 sum01 = _mm256_mul_ps(weight01, rhs_row01);
        const __m256 sum23 = _mm256_mul_ps(weight23, rhs_row23);
        const __m256 sum = _mm256_add_ps(sum01, sum23);

        const __m128 low = _mm256_castps256_ps128(sum);
        const __m128 high = _mm256_extractf128_ps(sum, 1);
        const __m128 row = _mm_add_ps(low, high);
        _mm_storeu_ps(out.m[i], row);
    }

    return out;
}
#endif

#if defined(H3D_MATH_SIMD_NEON)
static inline float32x4_t h3d_load_vec3_neon(h3d_vec3 v)
{
    const float in_data[4] = {v.x, v.y, v.z, 0.0f};
    return vld1q_f32(in_data);
}

static inline h3d_vec3 h3d_store_vec3_neon(float32x4_t v)
{
    float out_data[4];
    vst1q_f32(out_data, v);
    h3d_vec3 out = {out_data[0], out_data[1], out_data[2]};
    return out;
}

static inline float h3d_vec3_dot_neon(float32x4_t lhs, float32x4_t rhs)
{
    const float32x4_t mul = vmulq_f32(lhs, rhs);
#if defined(__aarch64__)
    return vaddvq_f32(mul);
#else
    float out_data[4];
    vst1q_f32(out_data, mul);
    return out_data[0] + out_data[1] + out_data[2];
#endif
}

static h3d_mat4 h3d_mat4_mul_neon(h3d_mat4 lhs, h3d_mat4 rhs)
{
    const float32x4_t rhs_row0 = vld1q_f32(rhs.m[0]);
    const float32x4_t rhs_row1 = vld1q_f32(rhs.m[1]);
    const float32x4_t rhs_row2 = vld1q_f32(rhs.m[2]);
    const float32x4_t rhs_row3 = vld1q_f32(rhs.m[3]);

    h3d_mat4 out;
    for (int i = 0; i < 4; ++i)
    {
        float32x4_t row = vmulq_n_f32(rhs_row0, lhs.m[i][0]);
        row = vmlaq_n_f32(row, rhs_row1, lhs.m[i][1]);
        row = vmlaq_n_f32(row, rhs_row2, lhs.m[i][2]);
        row = vmlaq_n_f32(row, rhs_row3, lhs.m[i][3]);
        vst1q_f32(out.m[i], row);
    }
    return out;
}
#endif

const char *h3d_math_simd_backend(void)
{
#if defined(H3D_MATH_SIMD_AVX)
    return "avx";
#elif defined(H3D_MATH_SIMD_SSE2)
    return "sse2";
#elif defined(H3D_MATH_SIMD_NEON)
    return "neon";
#else
    return "scalar";
#endif
}

h3d_vec3 h3d_vec3_add(h3d_vec3 lhs, h3d_vec3 rhs)
{
#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
    return h3d_store_vec3_x86(_mm_add_ps(h3d_load_vec3_x86(lhs), h3d_load_vec3_x86(rhs)));
#elif defined(H3D_MATH_SIMD_NEON)
    return h3d_store_vec3_neon(vaddq_f32(h3d_load_vec3_neon(lhs), h3d_load_vec3_neon(rhs)));
#else
    h3d_vec3 out = {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    return out;
#endif
}

h3d_vec3 h3d_vec3_sub(h3d_vec3 lhs, h3d_vec3 rhs)
{
#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
    return h3d_store_vec3_x86(_mm_sub_ps(h3d_load_vec3_x86(lhs), h3d_load_vec3_x86(rhs)));
#elif defined(H3D_MATH_SIMD_NEON)
    return h3d_store_vec3_neon(vsubq_f32(h3d_load_vec3_neon(lhs), h3d_load_vec3_neon(rhs)));
#else
    h3d_vec3 out = {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    return out;
#endif
}

h3d_vec3 h3d_vec3_mul_scalar(h3d_vec3 v, float s)
{
#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
    return h3d_store_vec3_x86(_mm_mul_ps(h3d_load_vec3_x86(v), _mm_set1_ps(s)));
#elif defined(H3D_MATH_SIMD_NEON)
    return h3d_store_vec3_neon(vmulq_n_f32(h3d_load_vec3_neon(v), s));
#else
    h3d_vec3 out = {v.x * s, v.y * s, v.z * s};
    return out;
#endif
}

h3d_vec3 h3d_vec3_div_scalar(h3d_vec3 v, float s)
{
    const float inv = 1.0f / s;
#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
    return h3d_store_vec3_x86(_mm_mul_ps(h3d_load_vec3_x86(v), _mm_set1_ps(inv)));
#elif defined(H3D_MATH_SIMD_NEON)
    return h3d_store_vec3_neon(vmulq_n_f32(h3d_load_vec3_neon(v), inv));
#else
    h3d_vec3 out = {v.x * inv, v.y * inv, v.z * inv};
    return out;
#endif
}

float h3d_vec3_dot(h3d_vec3 lhs, h3d_vec3 rhs)
{
#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
    return h3d_vec3_dot_x86(h3d_load_vec3_x86(lhs), h3d_load_vec3_x86(rhs));
#elif defined(H3D_MATH_SIMD_NEON)
    return h3d_vec3_dot_neon(h3d_load_vec3_neon(lhs), h3d_load_vec3_neon(rhs));
#else
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
#endif
}

h3d_vec3 h3d_vec3_cross(h3d_vec3 lhs, h3d_vec3 rhs)
{
#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
    return h3d_store_vec3_x86(h3d_vec3_cross_x86(h3d_load_vec3_x86(lhs), h3d_load_vec3_x86(rhs)));
#else
    h3d_vec3 out = {lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x};
    return out;
#endif
}

float h3d_vec3_length_sq(h3d_vec3 v)
{
    return h3d_vec3_dot(v, v);
}

float h3d_vec3_length(h3d_vec3 v)
{
    return sqrtf(h3d_vec3_length_sq(v));
}

h3d_vec3 h3d_vec3_normalize(h3d_vec3 v)
{
    const float len = h3d_vec3_length(v);
    if (len <= 0.0f)
    {
        h3d_vec3 zero = {0.0f, 0.0f, 0.0f};
        return zero;
    }
    return h3d_vec3_div_scalar(v, len);
}

h3d_mat4 h3d_mat4_identity(void)
{
    h3d_mat4 out;
    memset(&out, 0, sizeof(out));
    out.m[0][0] = 1.0f;
    out.m[1][1] = 1.0f;
    out.m[2][2] = 1.0f;
    out.m[3][3] = 1.0f;
    return out;
}

h3d_mat4 h3d_mat4_mul(h3d_mat4 lhs, h3d_mat4 rhs)
{
#if defined(H3D_MATH_SIMD_AVX)
    return h3d_mat4_mul_avx(lhs, rhs);
#elif defined(H3D_MATH_SIMD_SSE2)
    return h3d_mat4_mul_sse2(lhs, rhs);
#elif defined(H3D_MATH_SIMD_NEON)
    return h3d_mat4_mul_neon(lhs, rhs);
#else
    h3d_mat4 out;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            out.m[i][j] = lhs.m[i][0] * rhs.m[0][j] + lhs.m[i][1] * rhs.m[1][j] + lhs.m[i][2] * rhs.m[2][j] +
                          lhs.m[i][3] * rhs.m[3][j];
        }
    }
    return out;
#endif
}

h3d_mat4 h3d_mat4_transpose(h3d_mat4 m)
{
#if defined(H3D_MATH_SIMD_AVX) || defined(H3D_MATH_SIMD_SSE2)
    return h3d_mat4_transpose_sse2(m);
#else
    h3d_mat4 out;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            out.m[i][j] = m.m[j][i];
        }
    }
    return out;
#endif
}

static float h3d_det3(const h3d_mat4 *m, int r0, int r1, int r2, int c0, int c1, int c2)
{
    return m->m[r0][c0] * (m->m[r1][c1] * m->m[r2][c2] - m->m[r1][c2] * m->m[r2][c1]) -
           m->m[r0][c1] * (m->m[r1][c0] * m->m[r2][c2] - m->m[r1][c2] * m->m[r2][c0]) +
           m->m[r0][c2] * (m->m[r1][c0] * m->m[r2][c1] - m->m[r1][c1] * m->m[r2][c0]);
}

static float h3d_cofactor(const h3d_mat4 *m, int i, int j)
{
    int r[3];
    int c[3];
    for (int ri = 0, k = 0; k < 4; ++k)
    {
        if (k != i)
        {
            r[ri++] = k;
        }
    }
    for (int cj = 0, k = 0; k < 4; ++k)
    {
        if (k != j)
        {
            c[cj++] = k;
        }
    }

    const float sign = ((i + j) % 2 == 0) ? 1.0f : -1.0f;
    return sign * h3d_det3(m, r[0], r[1], r[2], c[0], c[1], c[2]);
}

h3d_mat4 h3d_mat4_invert(h3d_mat4 m)
{
    const float det = m.m[0][0] * h3d_cofactor(&m, 0, 0) + m.m[0][1] * h3d_cofactor(&m, 0, 1) +
                      m.m[0][2] * h3d_cofactor(&m, 0, 2) + m.m[0][3] * h3d_cofactor(&m, 0, 3);
    const float eps = 1e-10f;
    if (fabsf(det) < eps)
    {
        return h3d_mat4_identity();
    }

    const float inv_det = 1.0f / det;
    h3d_mat4 out;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            out.m[i][j] = h3d_cofactor(&m, j, i) * inv_det;
        }
    }
    return out;
}

h3d_mat4 h3d_mat4_look_at(h3d_vec3 eye, h3d_vec3 target, h3d_vec3 up)
{
    const h3d_vec3 z_axis = h3d_vec3_normalize(h3d_vec3_sub(target, eye));
    const h3d_vec3 x_axis = h3d_vec3_normalize(h3d_vec3_cross(up, z_axis));
    const h3d_vec3 y_axis = h3d_vec3_cross(z_axis, x_axis);

    h3d_mat4 out;
    memset(&out, 0, sizeof(out));

    out.m[0][0] = x_axis.x;
    out.m[0][1] = y_axis.x;
    out.m[0][2] = z_axis.x;
    out.m[0][3] = 0.0f;

    out.m[1][0] = x_axis.y;
    out.m[1][1] = y_axis.y;
    out.m[1][2] = z_axis.y;
    out.m[1][3] = 0.0f;

    out.m[2][0] = x_axis.z;
    out.m[2][1] = y_axis.z;
    out.m[2][2] = z_axis.z;
    out.m[2][3] = 0.0f;

    out.m[3][0] = -h3d_vec3_dot(x_axis, eye);
    out.m[3][1] = -h3d_vec3_dot(y_axis, eye);
    out.m[3][2] = -h3d_vec3_dot(z_axis, eye);
    out.m[3][3] = 1.0f;

    return out;
}

h3d_mat4 h3d_mat4_perspective_fov(float fov, float aspect_ratio, float near_plane, float far_plane)
{
    const float tan_half_fov = tanf(fov * 0.5f);
    const float range = far_plane - near_plane;

    h3d_mat4 out;
    memset(&out, 0, sizeof(out));
    out.m[0][0] = 1.0f / (aspect_ratio * tan_half_fov);
    out.m[1][1] = 1.0f / tan_half_fov;
    out.m[2][2] = -(far_plane + near_plane) / range;
    out.m[2][3] = -1.0f;
    out.m[3][2] = -(2.0f * far_plane * near_plane) / range;
    return out;
}

h3d_mat4 h3d_mat4_perspective(float width, float height, float near_plane, float far_plane)
{
    const float range = far_plane - near_plane;

    h3d_mat4 out;
    memset(&out, 0, sizeof(out));
    out.m[0][0] = 2.0f * near_plane / width;
    out.m[1][1] = 2.0f * near_plane / height;
    out.m[2][2] = -(far_plane + near_plane) / range;
    out.m[2][3] = -1.0f;
    out.m[3][2] = -(2.0f * far_plane * near_plane) / range;
    return out;
}

h3d_mat4 h3d_mat4_orthographic(float width, float height, float near_plane, float far_plane)
{
    const float range = far_plane - near_plane;

    h3d_mat4 out;
    memset(&out, 0, sizeof(out));
    out.m[0][0] = 2.0f / width;
    out.m[1][1] = 2.0f / height;
    out.m[2][2] = -2.0f / range;
    out.m[3][0] = -1.0f;
    out.m[3][1] = -1.0f;
    out.m[3][2] = -(far_plane + near_plane) / range;
    out.m[3][3] = 1.0f;
    return out;
}
