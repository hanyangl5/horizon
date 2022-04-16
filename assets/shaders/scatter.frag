// 1 in this shader represent 1000km

#version 450

#define PI 3.14159265359
#define eps 1e-6
#define MAX 1e6
#define TKM = 1000.0f

layout(location = 0) in vec3 world_pos;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D postion_depth;
layout(set = 2, binding = 1) uniform sampler2D normal_roughness;
layout(set = 2, binding = 2) uniform sampler2D albedo_metallic;

layout(set = 2, binding = 3) uniform CameraUb {
    vec3 eye_pos;
}camera_ub;



//  constants

struct ScatteringContext {
    vec3 light_dir;
    float light_intensity;
    vec3 planet_center;
    float planet_radius;
    float atmosphere_radius;
    vec2 rayliegh_mie_scale_height;
    float mie_g;
    vec3 view_dir;
    int view_scatter_nums, light_scatter_nums;
    vec3 beta_rayleigh;
    vec3 beta_mie;
};
// ------------------------------------------------------

float saturate(float x) {
    return clamp(x, 0.0f , 1.0f);
}

vec2 RaySphereIntersect(vec3 _origin, vec3 _dir, float _radius) {

	float b = dot( _origin, _dir );
	float c = dot( _origin, _origin ) - _radius * _radius;
	float d = b * b - c;

    vec2 min_max_intersections;
    if (d < 0.0f) {
        min_max_intersections = vec2(-1.0, -1.0);
    } else {
        d = sqrt(d);
        min_max_intersections = vec2(-b - d, -b + d);
    }
    return min_max_intersections;

}

vec3 ComputeIntersection(vec3 _origin, ScatteringContext _context, out vec2 intersection_point) {
    vec2 outer_intersectoin_point = RaySphereIntersect(_origin, _context.view_dir, _context.atmosphere_radius);
    // no intersection point 
    if (outer_intersectoin_point == vec2(-1.0, -1.0)) {
        return vec3(1.0, 0.0, 0.0);
    }

    vec2 inner_intersectoin_point = RaySphereIntersect(_origin, _context.view_dir, _context.planet_radius);
    
    // both intersected
    if (inner_intersectoin_point != vec2(-1.0, -1.0)) {
        intersection_point = vec2(outer_intersectoin_point.x, inner_intersectoin_point.x);
        return vec3(0.0, 1.0, 0.0);
    }
    
    intersection_point = outer_intersectoin_point;
    return vec3(0.0, 0.0, 1.0);
}

float PhaseRayleigh(float _c2) {
    float nom = 3.0f * (1 + _c2);
    float denom = 16.0f * PI;
    return nom / denom;
}

// g: (0.75, 0.9)
// henyey-greenstein phase function
float PhaseMieHG(float _g, float _c) {
    float g2 = _g * _g;
    float nom = 1.0f - g2;
    float denom = pow(1 + g2 - 2 * _g * _c, 1.5f);
    denom = denom * 4.0f * PI;
    return nom / denom;
}

// Cornette-Shanks phase function
float PhaseMieCS(float _g, float _c) {
    float g2 = _g * _g;
    float nom = 3.0f * (1.0f - g2) * (1.0f + _c * _c);
    float denom = pow(1 + g2 - 2 * _g * _c, 1.5f);
    denom = denom * 8.0f * PI * (2.0f + g2);
    return nom / denom;
}

// float AtmosphereDensity(float _height, float _radius, float _scale_height) {
//     return exp( -max(_height - _radius, 0.0f) / _scale_height);
// }

vec2 AtmosphereDensityRayMie(float _height, float _radius, vec2 _scale_height) {
    return exp( -max(_height - _radius, 0.0) / _scale_height);
}

// 
vec2 OpticalDepth(vec3 _sample_point_v, vec3 _intersection_point_l, ScatteringContext _context) {
    // start_v from sample point v, along light dir, to intersection point l
    vec3 step_size_l = (_intersection_point_l - _sample_point_v) / float(_context.light_scatter_nums);
    vec3 sample_point_l = _sample_point_v + step_size_l * 0.5;
    vec2 sum = vec2(0.0f);
    for(int i = 0; i < _context.light_scatter_nums; i++, sample_point_l += step_size_l) {
        sum += AtmosphereDensityRayMie(length(sample_point_l), _context.planet_radius, _context.rayliegh_mie_scale_height);
    }
    sum = sum * 1000.0f * length(step_size_l);
    return sum;
}

/*
cases:
if ray intersect with inner sphere s1 and outer sphere s2

cast 1: 2 intersection point with s1 and s2, integrate from surface to near intersection point 
cast 2: 0 intersection point with s1, 2 intersection point with s2, integrate from p1 to p2
cast 3: 0 intersection point with s1 and s2, discard
*/



vec3 InScattering(ScatteringContext _context) {

    vec3 sum_rayleigh = vec3( 0.0 );
    vec3 sum_mie = vec3( 0.0 );

    vec3 origin = camera_ub.eye_pos - _context.planet_center;
    
    vec2 view_sphere_intersection = vec2(0.0);
    // view dir atmoshphere intersection
    vec3 ret = ComputeIntersection(origin, _context, view_sphere_intersection);   
    // no intersection 
    if (view_sphere_intersection == vec2(0.0)) {
        return vec3(0.0f);
    }

    //return ret;

    vec3 start_v = origin + view_sphere_intersection.x * _context.view_dir;
    vec3 end_v = origin + view_sphere_intersection.y * _context.view_dir;
	vec3 step_size_v = (end_v - start_v) / float(_context.view_scatter_nums);
    float step_length_v = 1000.0f * length(step_size_v);

	vec3 sample_point_v = start_v + step_size_v * 0.5;

    vec2 optical_depth_v = vec2(0.0);
    // sample the view ray 
    for (int i = 0; i < _context.view_scatter_nums; i++, sample_point_v += step_size_v)
	{

        vec2 density_v = step_length_v * AtmosphereDensityRayMie(length(sample_point_v), _context.planet_radius, _context.rayliegh_mie_scale_height);
        optical_depth_v += density_v;

        // light dir atmosphere intersection
        vec2 light_sphere_intersection  = RaySphereIntersect(sample_point_v, _context.light_dir, _context.atmosphere_radius);

        vec3 intersection_point_l = sample_point_v + _context.light_dir * light_sphere_intersection.x;

        vec2 optical_depth_l = OpticalDepth(sample_point_v, intersection_point_l, _context);
        optical_depth_l = vec2(0.0);

        vec3 attenuation = 
        exp( - ( optical_depth_v.x + optical_depth_l.x ) * _context.beta_rayleigh 
             - ( optical_depth_v.y + optical_depth_l.y ) * _context.beta_mie);
        
		sum_rayleigh += density_v.x * attenuation;
        sum_mie += density_v.y * attenuation;

	}

    float theta = dot(_context.view_dir, _context.light_dir);
    float theta2 = theta * theta;
    float phase_rayleigh = PhaseRayleigh(theta2);
    float phase_mie = PhaseMieCS(theta, _context.mie_g);
    return  (sum_rayleigh * _context.beta_rayleigh * phase_rayleigh + 
             sum_mie * _context.beta_mie      * phase_mie) * 
             _context.light_intensity;

}

vec3 GetCameraVector(vec2 resolution, vec2 coord){
    	vec2 uv = coord.xy / resolution.xy - vec2(0.5);
         uv.x *= resolution.x / resolution.y;

    return normalize(vec3(uv.x, uv.y, -1.0));
}


void main() {

    ScatteringContext context;
    context.light_dir = vec3(0.0, 0.0, -1.0);
    context.light_intensity = 10.0f;
    context.planet_center = vec3(0.0f);
    context.planet_radius = 6378e0;
    context.atmosphere_radius = 6478e0;
    context.view_scatter_nums = 64;
    context.light_scatter_nums = 32;
    // thickness of the atmosphere if its density were uniform (how far to go up before the scattering has no effect)
    context.rayliegh_mie_scale_height = vec2(8e0, 1.2e0);
    // scattering coeffs at sea level
    context.beta_rayleigh = vec3(5.5e-6, 13.0e-6, 22.4e-6);
    context.beta_mie = vec3(21e-6);
    context.mie_g = 0.7;

    context.view_dir = normalize(world_pos - camera_ub.eye_pos);
    //context.view_dir = GetCameraVector(vec2(1920.0,1080.0), gl_FragCoord.xy); 
    vec3 scatter = InScattering(context);
    out_color = vec4(scatter, 1.0);
    //out_color += texture(albedo_metallic, gl_FragCoord.xy / vec2(1920.f, 1080.f));
}