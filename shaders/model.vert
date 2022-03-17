#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
// layout(location = 3) in vec3 inTangent;
// layout(location = 4) in vec3 inBiTangent;

layout(location = 0) out vec2 fragTexCoord;

// scene descriptorset

layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
} sceneUb;

layout(set = 0, binding = 1) uniform SceneUb2 {
    float a;
    float b;
} sceneUb2;


// mesh descriptorset

layout(set = 2, binding = 0) uniform MeshUb {
    mat4 model;
} meshUb;



void main() {
    fragTexCoord = inTexcoord;
    gl_Position = sceneUb.proj * sceneUb.view * meshUb.model * vec4(inPosition, 1.0);

}
