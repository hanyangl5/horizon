#version 450

layout(location = 0) in vec3 outNormal;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
} sceneUb;

void main() {
    outColor = sceneUb.view*vec4(outNormal,1.0);
}