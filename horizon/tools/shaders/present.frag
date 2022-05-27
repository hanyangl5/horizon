#version 450

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D color_texture;

void main() {
    vec2 frag_coord = gl_FragCoord.xy / vec2(1920.0,1080.0);
    outColor = texture(color_texture, frag_coord);
}