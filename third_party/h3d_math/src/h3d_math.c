#include "h3d_math.h"

#include <math.h>
#include <string.h>

h3d_vec3 h3d_vec3_add(h3d_vec3 lhs, h3d_vec3 rhs)
{
    h3d_vec3 out = {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    return out;
}

h3d_vec3 h3d_vec3_sub(h3d_vec3 lhs, h3d_vec3 rhs)
{
    h3d_vec3 out = {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    return out;
}

h3d_vec3 h3d_vec3_mul_scalar(h3d_vec3 v, float s)
{
    h3d_vec3 out = {v.x * s, v.y * s, v.z * s};
    return out;
}

h3d_vec3 h3d_vec3_div_scalar(h3d_vec3 v, float s)
{
    const float inv = 1.0f / s;
    h3d_vec3 out = {v.x * inv, v.y * inv, v.z * inv};
    return out;
}

float h3d_vec3_dot(h3d_vec3 lhs, h3d_vec3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

h3d_vec3 h3d_vec3_cross(h3d_vec3 lhs, h3d_vec3 rhs)
{
    h3d_vec3 out = {lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x};
    return out;
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
}

h3d_mat4 h3d_mat4_transpose(h3d_mat4 m)
{
    h3d_mat4 out;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            out.m[i][j] = m.m[j][i];
        }
    }
    return out;
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
