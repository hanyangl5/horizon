
#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding =0) uniform uboStruct{
    float a;
    float b;
    float c;
}ubo;

void main() {
    outColor = vec4(ubo.a, ubo.b, ubo.c, 1.0);
}