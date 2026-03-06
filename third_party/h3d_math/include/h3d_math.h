#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float x;
    float y;
} h3d_vec2;

typedef struct
{
    float x;
    float y;
    float z;
} h3d_vec3;

typedef struct
{
    float x;
    float y;
    float z;
    float w;
} h3d_vec4;

typedef struct
{
    float m[4][4];
} h3d_mat4;

const char *h3d_math_simd_backend(void);

h3d_vec3 h3d_vec3_add(h3d_vec3 lhs, h3d_vec3 rhs);
h3d_vec3 h3d_vec3_sub(h3d_vec3 lhs, h3d_vec3 rhs);
h3d_vec3 h3d_vec3_mul_scalar(h3d_vec3 v, float s);
h3d_vec3 h3d_vec3_div_scalar(h3d_vec3 v, float s);

float h3d_vec3_dot(h3d_vec3 lhs, h3d_vec3 rhs);
h3d_vec3 h3d_vec3_cross(h3d_vec3 lhs, h3d_vec3 rhs);
float h3d_vec3_length(h3d_vec3 v);
float h3d_vec3_length_sq(h3d_vec3 v);
h3d_vec3 h3d_vec3_normalize(h3d_vec3 v);

h3d_mat4 h3d_mat4_identity(void);
h3d_mat4 h3d_mat4_mul(h3d_mat4 lhs, h3d_mat4 rhs);
h3d_mat4 h3d_mat4_transpose(h3d_mat4 m);
h3d_mat4 h3d_mat4_invert(h3d_mat4 m);

h3d_mat4 h3d_mat4_look_at(h3d_vec3 eye, h3d_vec3 target, h3d_vec3 up);
h3d_mat4 h3d_mat4_perspective_fov(float fov, float aspect_ratio, float near_plane, float far_plane);
h3d_mat4 h3d_mat4_perspective(float width, float height, float near_plane, float far_plane);
h3d_mat4 h3d_mat4_orthographic(float width, float height, float near_plane, float far_plane);

#ifdef __cplusplus
}
#endif
