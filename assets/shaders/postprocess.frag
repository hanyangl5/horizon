#version 450

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D color_texture;

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
	vec3 color = texture(color_texture, frag_tex_coord).xyz;
	color = TonemapACES(color);
	//color = GammaCorrection(color);
    out_color = vec4(color, 1.0);
}