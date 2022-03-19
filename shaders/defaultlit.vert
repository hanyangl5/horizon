#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
// layout(location = 3) in vec3 inTangent;
// layout(location = 4) in vec3 inBiTangent;

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec2 fragTexCoord;

// set 0: scene

layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
} sceneUb;

// set 1: material

// set 2: mesh

layout(set = 2, binding = 0) uniform MeshUb {
    mat4 model;
} meshUb;



void main() {
    fragTexCoord = inTexcoord;
    gl_Position = sceneUb.proj * sceneUb.view * meshUb.model * vec4(inPosition, 1.0);

}
