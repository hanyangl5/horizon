#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
} sceneUb;


// material descriptorset

layout(set = 1, binding = 0) uniform MaterialParams {
    vec4 bcFactor;
    vec2 mrFactor;
}materialParams;

layout(set = 1, binding = 1) uniform sampler2D bcTexture;
layout(set = 1, binding = 2) uniform sampler2D normalTexture;
layout(set = 1, binding = 3) uniform sampler2D mrTexture;

void main() {
    outColor = texture(bcTexture, fragTexCoord);
}