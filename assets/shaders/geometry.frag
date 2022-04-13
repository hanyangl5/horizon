#version 450

layout(location = 0) in vec3 world_pos;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec2 frag_tex_coord;

layout(location = 0) out vec4 position_depth;
layout(location = 1) out vec4 normal_roughness;
layout(location = 2) out vec4 albedo_metallic;

// set 0: scene
layout(set = 0, binding = 0) uniform SceneUb {
    mat4 view, proj;
    vec2 near_far;
} scene_ub;
// set 1: material

layout(set = 1, binding = 0) uniform MaterialParams {
    bool has_base_color;
    bool has_normal;
    bool has_metallic_roughness;
    // vec4 bcFactor;
    // vec3 normalFactor;
    // vec2 mrFactor;
}material_params;

layout(set = 1, binding = 1) uniform sampler2D base_color_texture;
layout(set = 1, binding = 2) uniform sampler2D normal_texture;
layout(set = 1, binding = 3) uniform sampler2D metallic_roughness_texture;
// set 2: mesh

// -------------------------------------------------------


float ToLinearDepth(float _depth)
{
    float near_plane = scene_ub.near_far.x;
    float far_plane = scene_ub.near_far.y;
	float z = _depth * 2.0f - 1.0f; 
	return (2.0f * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));	
}


void main() {
    
    vec3 albedo = material_params.has_base_color ? texture(base_color_texture, frag_tex_coord).xyz : vec3(1.0);
    vec3 normal = material_params.has_normal? texture(normal_texture, frag_tex_coord).xyz : vec3(0.0);
    float metallic= material_params.has_metallic_roughness ? texture(metallic_roughness_texture, frag_tex_coord).x : 0.0f;
    float roughness = material_params.has_metallic_roughness ? texture(metallic_roughness_texture, frag_tex_coord).y : 1.0f;
    
    position_depth = vec4(world_pos, ToLinearDepth(gl_FragCoord.z));
    normal_roughness = vec4(normalize(world_normal), roughness);
    albedo_metallic = vec4(albedo, roughness);
    
}