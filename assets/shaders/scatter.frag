#version 450

#define PI 3.14159265359
#define eps 1e-6
#define MAX 1e6

const vec3 k_ray = vec3( 3.8, 13.5, 33.1 );
const vec3 k_mie = vec3( 21.0 );
const float k_mie_ex = 1.1;

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
    vec3 planet_center;
    float planet_radius;
    float atmosphere_radius;
    vec2 rayliegh_mie_scale_height;
    float mie_g;
    vec3 view_dir;
    int k_scatter_nums_view, k_scatter_nums_light;
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
    if(d < 0.0f){
        min_max_intersections = vec2(-1.0, -1.0);
    }
    else 
    {
        d = sqrt(d);
        min_max_intersections = vec2( -b - d, -b + d);
    }
    return min_max_intersections;

}

vec3 ComputeIntersection(vec3 _origin, ScatteringContext _context, out vec2 intersection_point) {
    vec2 outer_intersectoin_point = RaySphereIntersect(_origin, _context.view_dir, _context.atmosphere_radius);
    // no intersection point 
    if(outer_intersectoin_point.y < 0.0f) {
        return vec3(1.0, 0.0, 0.0);
    }

    vec2 inner_intersectoin_point = RaySphereIntersect(_origin, _context.view_dir, _context.planet_radius);
    
    // both intersected
    if(inner_intersectoin_point.x > 0.0f) {
        //outer_intersectoin_point.y = inner_intersectoin_point.x;
        intersection_point = vec2(outer_intersectoin_point.x, inner_intersectoin_point.y);
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

float AtmosphereDensity(float _height, float _radius, float _scale_height) {
    return exp( -max(_height - _radius, 0.0f) / _scale_height);
}

vec2 AtmosphereDensityRayMie(float _height, float _radius, vec2 _scale_height) {
    return exp( -max(_height - _radius, 0.0f) / _scale_height);
}

// 
vec2 OpticalDepth(vec3 _sample_point_v, vec3 _intersection_point_l, ScatteringContext _context) {
    // start from sample point v, along light dir, to intersection point l
    vec3 step_size_l = (_intersection_point_l - _sample_point_v) / float(_context.k_scatter_nums_light);
    vec3 sample_point_l = _sample_point_v + step_size_l * 0.5;
    vec2 sum = vec2(0.0f);
    for(int i = 0; i < _context.k_scatter_nums_light; i++, sample_point_l += step_size_l) {
        sum += AtmosphereDensityRayMie(length(sample_point_l), _context.planet_radius, _context.rayliegh_mie_scale_height);
        
    }
    sum *= length(step_size_l);
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

    vec3 origin = camera_ub.eye_pos - _context.planet_center;
    
    vec2 intersection_info = vec2(0.0);
    vec3 ret = ComputeIntersection(origin, _context, intersection_info);
    return ret;
    //if(intersection_info == vec2(0.0f)) return vec3(0.0f);

    float len = (intersection_info.y - intersection_info.x) / float(_context.k_scatter_nums_view);
    vec3 start = origin + intersection_info.x * _context.view_dir;
    vec3 end = origin + intersection_info.y * _context.view_dir;

	vec3 step_size_v = (end - start) / float(_context.k_scatter_nums_view);
	//float step_length = length(step_size);

	vec3 sample_point_v = start + step_size_v * 0.5;

    vec2 optical_depth_rayleigh_mie_v = vec2(0.0);

	vec3 sum_ray = vec3( 0.0 );
    vec3 sum_mie = vec3( 0.0 );

    // sample the view ray 
    for (int i = 0; i < _context.k_scatter_nums_view; i++, sample_point_v += step_size_v)
	{

        float height_v = length(sample_point_v);
        vec2 density_ray_mie_v = len * AtmosphereDensityRayMie(height_v, _context.planet_radius, _context.rayliegh_mie_scale_height);
        optical_depth_rayleigh_mie_v += density_ray_mie_v;

        float a = dot(_context.light_dir, _context.light_dir);
        float b = 2.0 * dot(_context.light_dir, sample_point_v);
        float c = dot(sample_point_v, sample_point_v) - (_context.atmosphere_radius * _context.atmosphere_radius);
        float d = (b * b) - 4.0 * a * c;

        vec2 light_sphere_intersection  = RaySphereIntersect(sample_point_v, _context.light_dir, _context.atmosphere_radius);

        vec3 intersection_point_l = sample_point_v + _context.light_dir * light_sphere_intersection.y;

        vec2 optical_depth_rayleigh_mie_l = OpticalDepth(sample_point_v, intersection_point_l, _context);
 

        vec3 attenuation = exp( - ( optical_depth_rayleigh_mie_v.x + optical_depth_rayleigh_mie_l.x ) * k_ray - ( optical_depth_rayleigh_mie_v.y + optical_depth_rayleigh_mie_l.y ) * k_mie * k_mie_ex );
        
		sum_ray += density_ray_mie_v.x * attenuation;
        sum_mie += density_ray_mie_v.y * attenuation;

	}

    float theta = dot(_context.view_dir, _context.light_dir);
    float theta2 = theta * theta;
    float phase_rayleigh = PhaseRayleigh(theta2);
    float phase_mie = PhaseMieCS(theta, _context.mie_g);

    return sum_ray * k_ray * phase_rayleigh + sum_mie * k_mie * phase_mie;

}



void main() {

    ScatteringContext context;
    context.light_dir = vec3(0.0, 0.0, -1.0);
    context.planet_center = vec3(0.0f);
    context.planet_radius = 6378.0f;
    context.atmosphere_radius = 8478.0f;
    context.rayliegh_mie_scale_height = vec2(8.0f, 1.2f);
    context.k_scatter_nums_view = 32;
    context.k_scatter_nums_light = 32;
    context.view_dir =  normalize(world_pos - camera_ub.eye_pos);

    vec3 scatter = InScattering(context);
    out_color = vec4(scatter,1.0);// + texture(albedo_metallic, gl_FragCoord.xy / vec2(1920.f, 1080.f));
}