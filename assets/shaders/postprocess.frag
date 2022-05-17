#version 450

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D color_texture;

float sRGB(float x)
{
	if (x <= 0.00031308)
		return 12.92 * x;
	else
		return 1.055*pow(x, (1.0 / 2.4)) - 0.055;
}

vec4 sRGB(vec4 vec)
{
	return vec4(sRGB(vec.x), sRGB(vec.y), sRGB(vec.z), vec.w);
}


vec3 TonemapACES(vec3 x)
{
	const float A = 2.51f;
	const float B = 0.03f;
	const float C = 2.43f;
	const float D = 0.59f;
	const float E = 0.14f;
	return (x * (A * x + B)) / (x * (C * x + D) + E);
}

vec3 GammaCorrection(vec3 x){
	return pow( x, vec3( 1.0 / 2.2 ));
}

void main() {
	out_color = texture(color_texture, frag_tex_coord);
	vec4 rgbA = texture(color_texture, frag_tex_coord);
	rgbA /= rgbA.aaaa;	// Normalise according to sample count when path tracing
	vec3 white_point = vec3(1.08241, 0.96756, 0.95003);
	float exposure = 10.0;
	out_color = vec4( pow(vec3(1.0) - exp(-rgbA.rgb / white_point * exposure), vec3(1.0 / 2.2)), 1.0 );
}