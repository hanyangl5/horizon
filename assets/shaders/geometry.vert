#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;
// layout(location = 3) in vec3 inTangent;
// layout(location = 4) in vec3 inBiTangent;

layout(location = 0) out vec3 world_pos;
layout(location = 1) out vec3 world_normal;
layout(location = 2) out vec2 frag_tex_coord;

// set 0: scene

layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
    vec2 near_far;
} scene_ub;

// set 1: material

// push constant

layout(push_constant) uniform MeshUb {
    mat4 model;
} mesh_ub;


void main() {
    mat4 model = mesh_ub.model;
    world_pos = (model * vec4(in_position, 1.0)).xyz;
    world_normal = (model * vec4(in_normal, 0.0)).xyz;
    frag_tex_coord = in_tex_coord;
    gl_Position = scene_ub.proj * scene_ub.view * model * vec4(in_position, 1.0);
    
}
