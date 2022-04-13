#version 450

layout(location = 0) in vec3 in_position;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec2 frag_tex_coord;

void main() {
    frag_tex_coord = in_tex_coord;
    gl_Position = vec4(in_position, 1.0);
}