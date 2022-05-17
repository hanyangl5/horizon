// 1 in this shader represent 1000km

#version 450

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform ScatteringUb {
    mat4 inv_view_projection_matrix;
    vec2 resolution;
    vec2 pad0;
    vec3 camera_position;
    float pad1;
} scattering_ub;

layout(set = 0, binding = 1) uniform sampler2D transmittance_lut;
layout(set = 0, binding = 2) uniform sampler3D scattering_lut;
layout(set = 0, binding = 3) uniform sampler2D geometry_color;
layout(set = 0, binding = 4) uniform sampler2D scene_depth;

#include "functions.glsl"

void main() {
    AtmosphereParameters atmosphere = GetAtmosphereParameters();
	vec2 frag_coord = gl_FragCoord.xy / vec2(scattering_ub.resolution);
    vec3 x_clip = vec3(frag_coord * vec2(2.0, -2.0) - vec2(1.0, -1.0), 0.5); 
    vec4 _x_world = scattering_ub.inv_view_projection_matrix * vec4(x_clip, 1.0); 
	vec3 x_world = _x_world.xyz / _x_world.w;
    vec3 view_dir = normalize(x_world - scattering_ub.camera_position);
    vec3 sun_dir = - normalize(vec3(0.0, -1.0, -1.0));

	vec3 SunLuminance = vec3(0.0); 
	// render light source
	if (dot(view_dir, sun_dir) > cos(0.5*0.505*3.14159 / 180.0)) \
	{ 
		SunLuminance = vec3(100.0); /*Using any value here because everything is relative anyway in this simulation*/ \
	} 

    float earth_radius = atmosphere.bottom_radius; 
    vec3 earth_pos = vec3(0.0, 0.0, 0.0); 
    vec3 cam_pos = scattering_ub.camera_position;// + vec3(0, 0, earth_radius); 


	vec3 transmittance = vec3(0.0);

	float t = raySphereIntersect(scattering_ub.camera_position, view_dir, earth_pos, earth_radius);
	
	// don't calculate atmosphere before scene depth;
	x_clip.z = texture(scene_depth, frag_coord).r;
	if (x_clip.z > 0.0f)
	{
		vec4 DepthBufferWorldPos = scattering_ub.inv_view_projection_matrix * vec4(x_clip,1.0);
		DepthBufferWorldPos /= DepthBufferWorldPos.w;
		float tDepth = length(DepthBufferWorldPos.xyz - cam_pos); // not camPos, no earth offset
		if (t < 0.0f || tDepth < t)
		{
			t = tDepth;
		}
	}

	float shadow_length = 0.0f; 
	vec3 SunIlluminanceToGroundLuminanceTransfer = vec3(0.0);
	vec3 SunIlluminanceToSkyLuminanceTransfer = vec3(0.0);
	vec3 SunTransmittance = vec3(0.0);
	// intersect with ground
	if (t > 0.0)
	{
		vec3 world_pos = cam_pos + t * view_dir; // intersection point 
		SunIlluminanceToGroundLuminanceTransfer = GetSkyRadianceToPoint(atmosphere, transmittance_lut, scattering_lut, scattering_lut, cam_pos, world_pos, shadow_length, sun_dir, transmittance);
	}
	else
	{
		//intersect with atmosphere
		SunIlluminanceToSkyLuminanceTransfer = GetSkyRadiance(atmosphere, transmittance_lut, scattering_lut, scattering_lut, cam_pos, view_dir, shadow_length, sun_dir, transmittance);
		SunTransmittance = transmittance;
	}
	transmittance = vec3(0.0);
	// Compute in scattering and apply transmittance on background
	vec3 luminance = (SunIlluminanceToSkyLuminanceTransfer + SunIlluminanceToGroundLuminanceTransfer) + SunLuminance * SunTransmittance;
	out_color = texture(geometry_color, frag_coord) + vec4(luminance, 1.0 - dot(transmittance, vec3(0.33, 0.33, 0.34)));
}