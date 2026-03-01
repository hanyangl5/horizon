#include "include/common/luminance.h"
#include "include/common/hlsl_common.h"

// Set 0: Per-frame resources
Texture2D<float4> prev_color_tex;
Texture2D<float4> curr_color_tex;
Texture2D<float2> mv_tex;
[[vk::image_format("rgba8")]] RWTexture2D<float4> out_color_tex;

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint width, height;
    curr_color_tex.GetDimensions(width, height);
    if (threadID.x >= width || threadID.y >= height)
        return;

    int2 max_coord = int2(width, height) - int2(1, 1);
    int2 curr_xy = int2(threadID.xy);
    int3 currCoord = int3(curr_xy, 0);
    float3 curr_color = curr_color_tex.Load(currCoord).xyz;
    float2 motion_vector = mv_tex.Load(currCoord).xy;
    int2 prevCoord = curr_xy - int2(motion_vector * float2(width, height));
    prevCoord = clamp(prevCoord, int2(0, 0), max_coord);
    float3 prev_color = prev_color_tex.Load(int3(prevCoord, 0)).xyz;

    int2 p0 = clamp(curr_xy + int2(1, 0), int2(0, 0), max_coord);
    int2 p1 = clamp(curr_xy + int2(-1, 0), int2(0, 0), max_coord);
    int2 p2 = clamp(curr_xy + int2(0, 1), int2(0, 0), max_coord);
    int2 p3 = clamp(curr_xy + int2(0, -1), int2(0, 0), max_coord);
    float3 c0 = curr_color_tex.Load(int3(p0, 0)).xyz;
    float3 c1 = curr_color_tex.Load(int3(p1, 0)).xyz;
    float3 c2 = curr_color_tex.Load(int3(p2, 0)).xyz;
    float3 c3 = curr_color_tex.Load(int3(p3, 0)).xyz;
    float3 c_min = min(curr_color, min(c0, min(c1, min(c2, c3))));
    float3 c_max = max(curr_color, max(c0, max(c1, max(c2, c3))));
    prev_color = clamp(prev_color, c_min, c_max);

    float w0 = Luminance(prev_color) * 0.95;
    float w1 = Luminance(curr_color) * 0.05;
    float w = w1 / (w0 + w1);
    out_color_tex[threadID.xy] = float4(lerp(prev_color, curr_color, w), 1.0);
}
