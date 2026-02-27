#include "include/common/luminance.h"
#include "include/common/hlsl_common.h"

// Set 0: Per-frame resources
Texture2D<float4> prev_color_tex;
Texture2D<float4> curr_color_tex;
Texture2D<float2> mv_tex;
[[vk::image_format("rgba8")]] RWTexture2D<float4> out_color_tex;

static const float2 kResolution = float2(1600, 900);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    int3 currCoord = int3(threadID.xy, 0);
    float3 curr_color = curr_color_tex.Load(currCoord).xyz;
    float2 motion_vector = mv_tex.Load(currCoord).xy;
    int2 prevCoord = (int2)threadID.xy - (int2)(motion_vector * kResolution);
    float3 prev_color = prev_color_tex.Load(int3(prevCoord, 0)).xyz;

    float3 c0 = curr_color_tex.Load(int3((int2)threadID.xy + int2(1, 0), 0)).xyz;
    float3 c1 = curr_color_tex.Load(int3((int2)threadID.xy + int2(-1, 0), 0)).xyz;
    float3 c2 = curr_color_tex.Load(int3((int2)threadID.xy + int2(0, 1), 0)).xyz;
    float3 c3 = curr_color_tex.Load(int3((int2)threadID.xy + int2(0, -1), 0)).xyz;
    float3 c_min = min(curr_color, min(c0, min(c1, min(c2, c3))));
    float3 c_max = max(curr_color, max(c0, max(c1, max(c2, c3))));
    prev_color = clamp(prev_color, c_min, c_max);

    float w0 = Luminance(prev_color) * 0.95;
    float w1 = Luminance(curr_color) * 0.05;
    float w = w1 / (w0 + w1);
    out_color_tex[threadID.xy] = float4(lerp(prev_color, curr_color, w), 1.0);
}
