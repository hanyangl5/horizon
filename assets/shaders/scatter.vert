#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

layout(location = 0) out vec3 worldPos;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec2 fragTexCoord;

// set 0: scene

layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
    vec2 nearFar;
} sceneUb;


layout(push_constant) uniform MeshUb {
    mat4 model;
} mesh;


void main() {
    mat4 model = mesh.model;
    worldPos = (model * vec4(inPosition, 1.0)).xyz;
    worldNormal = (model * vec4(inNormal, 1.0)).xyz;
    fragTexCoord = inTexcoord;
    gl_Position = sceneUb.proj * sceneUb.view * model * vec4(inPosition, 1.0);
    
}
