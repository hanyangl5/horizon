#version 450

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D color_texture;

void main() {
    outColor = texture(color_texture, frag_tex_coord);
}