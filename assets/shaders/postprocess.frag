#version 450

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

float Exposure(float aperture, float shutterSpeed, float sensitivity) {
	float ev100 = log2((aperture * aperture) / shutterSpeed * 100.0 / sensitivity);
    return 1.0 / (pow(2.0, ev100));
}


void main() {
	float aperture = 1, shutterSpeed = 1000, ISO = 1;
	vec2 frag_coord = gl_FragCoord.xy / vec2(1920.0,1080.0);
	vec3 color = texture(color_texture, frag_coord).rgb;
	float exposure = Exposure(aperture, shutterSpeed, ISO);
	color *= exposure;
	color = TonemapACES(color);
	out_color = vec4( color, 1.0 );
}