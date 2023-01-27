#include "include/common/luminance.hlsl"
RES(RTex2D(float4), prev_color_tex, UPDATE_FREQ_PER_FRAME, u0, binding = 0);
RES(RTex2D(float4), curr_color_tex, UPDATE_FREQ_PER_FRAME, u1, binding = 1);
RES(RTex2D(float2), mv_tex, UPDATE_FREQ_PER_FRAME, u2, binding = 2);
RES(RWTexture2D<float4>, out_color_tex, UPDATE_FREQ_PER_FRAME, u1, binding = 3);

CBUFFER(SceneConstants, UPDATE_FREQ_PER_FRAME, b1, binding = 5)
{
    DATA(float4x4, camera_view, None);
    DATA(float4x4, camera_projection, None);
    DATA(float4x4, camera_view_projection, None);
    DATA(float4x4, camera_inverse_view_projection, None);
    DATA(uint2, resolution, None);
    DATA(uint2, pad0, None);
    DATA(float3, camera_pos, None);
    DATA(uint, pad1, None);
    DATA(float, ibl_intensity, None);
};


NUM_THREADS(8, 8, 1)
void CS_MAIN( uint3 thread_id: SV_DispatchThreadID) 
{
    INIT_MAIN;

    float3 curr_color  = LoadRWTex2D(Get(curr_color_tex), thread_id.xy).xyz;

    float2 motion_vector  = LoadRWTex2D(Get(mv_tex), thread_id.xy).xy;

    uint2 previous_sample = thread_id.xy - uint2(motion_vector * Get(resolution));
    float3 prev_color  = LoadRWTex2D(Get(prev_color_tex), previous_sample).xyz;

    // Apply clamping on the history color.
    float3 c0 = LoadRWTex2D(Get(curr_color_tex), thread_id.xy + int2(1, 0)).xyz;
    float3 c1 = LoadRWTex2D(Get(curr_color_tex), thread_id.xy + int2(-1, 0)).xyz;
    float3 c2 = LoadRWTex2D(Get(curr_color_tex), thread_id.xy + int2(0, 1)).xyz;
    float3 c3 = LoadRWTex2D(Get(curr_color_tex), thread_id.xy + int2(0, -1)).xyz;
    
    float3 c_min = fast_min(curr_color, fast_min(c0, fast_min(c1, fast_min(c2, c3))));
    float3 c_max = fast_max(curr_color, fast_max(c0, fast_max(c1, fast_max(c2, c3))));;
    
    prev_color = clamp(prev_color, c_min, c_max);

    // Karis, Brian. "High-quality temporal supersampling, result blur
    float w0 = Luminance(prev_color) * (1.0 - 0.05);
    float w1 = Luminance(curr_color) * 0.05;
    float w = w1 / (w0 + w1);

    float4 final_color = float4((1.0 - w) * prev_color + w * curr_color, 1.0);

    Write2D(Get(out_color_tex), thread_id.xy, final_color);

    RETURN();
}

