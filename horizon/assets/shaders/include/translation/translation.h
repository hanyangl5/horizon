float3 ReconstructWorldPos(float4x4 inverse_vp, float depth, vec2 fragCoord) {
    float2 ndc = float2(fragCoord.x * 2.0 - 1.0, 1.0 - fragCoord.y * 2.0); // unify this in different graphic api
    float4 pos =  mul(float4(ndc, depth, 1.0), inverse_vp);
    pos.xyz /= pos.w;
    return pos.xyz; 
}