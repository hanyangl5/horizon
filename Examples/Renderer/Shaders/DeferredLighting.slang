// Cookbook deferred lighting adapted to Horizon's GBuffer; see ../Cookbook-LICENSE.txt.
#include "Common.slang"
#include "LightingBRDF.slang"
#include "Math.slang"

cbuffer RootConstant0
{
    uint frameIndex;
    uint lightingFrameIndex;
    uint emissiveIndex;
    uint normalMaterialIndex;
    uint baseColorMetallicIndex;
    uint depthIndex;
    uint shadowMapIndex;
    uint shadowSamplerIndex;
    uint environmentIndex;
    uint irradianceIndex;
    uint brdfLutIndex;
    uint environmentSamplerIndex;
    uint rtShadowAOIndex;
    uint rtReflectionIndex;
    uint ssaoIndex;
};
struct PSInput
{
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};
PSInput VSMain(uint vertexId : SV_VertexID)
{
    const float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    PSInput      output;
    output.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    output.uv = uv;
    return output;
}
// Depth-aware upsample of the reduced-resolution ray traced results: bilinear weights over the four nearest texels,
// each scaled down by its source pixel's relative view distance difference, so the results do not bleed across edges.
static const float kRTUpsampleDepthScale = 50.0;
struct RTUpsample
{
    int2  taps[4];
    float weights[4];
};
RTUpsample rtUpsample(int2 pixel, int2 size, int2 lowSize, float viewDistance, Texture2D<float> depthTexture, FrameData frame)
{
    const float2 position = float2(pixel) * float2(lowSize) / float2(size);
    const int2   base = int2(floor(position));
    const float2 fraction = position - float2(base);
    RTUpsample   result;
    float        weightSum = 0.0;
    float        closest = 1e30;
    int          closestTap = 0;
    for (int i = 0; i < 4; ++i)
    {
        const int2   offset = int2(i & 1, i >> 1);
        const int2   tap = min(base + offset, lowSize - 1);
        const int2   source = rtSourcePixel(tap, lowSize, size);
        const float  tapDepth = depthTexture.Load(int3(source, 0));
        const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));
        const float  difference =
            tapDepth >= 1.0
                ? 1e30
                : abs(length(reconstructWorldPosition((float2(source) + 0.5) / float2(size), tapDepth, frame.inverseViewProjection) -
                             frame.eye.xyz) - viewDistance) / viewDistance;
        result.taps[i] = tap;
        result.weights[i] = bilinear.x * bilinear.y * exp(-difference * kRTUpsampleDepthScale);
        weightSum += result.weights[i];
        if (difference < closest)
        {
            closest = difference;
            closestTap = i;
        }
    }
    for (int i = 0; i < 4; ++i)
    {
        // Thin geometry may match none of the texels; it takes the nearest in depth instead of a blend of the background.
        result.weights[i] = weightSum > 1e-4 ? result.weights[i] / weightSum : float(i == closestTap);
    }
    return result;
}

float4 PSMain(PSInput input): SV_Target0
{
    const int3        pixel = int3(int2(input.position.xy), 0);
    Texture2D<float4> emissive = ResourceDescriptorHeap[emissiveIndex];
    Texture2D<float>  depthTexture = ResourceDescriptorHeap[depthIndex];
    half3             scene = half3(emissive.Load(pixel).rgb);
    float             depth = depthTexture.Load(pixel);
    // The depth target clears to 1.0 and the projection maps the far plane there, so only the cleared
    // value means "no geometry". Anything below it is lit, however far away the far plane puts it.
    if (depth >= 1.0)
    {
        return float4(float3(scene), 1.0);
    }

    StructuredBuffer<FrameData>         frame = ResourceDescriptorHeap[frameIndex];
    StructuredBuffer<LightingFrameData> lightBuffer = ResourceDescriptorHeap[lightingFrameIndex];
    LightingFrameData                   light = lightBuffer[0];
    Texture2D<float4>                   normalMaterial = ResourceDescriptorHeap[normalMaterialIndex];
    Texture2D<float4>                   baseColorMetallic = ResourceDescriptorHeap[baseColorMetallicIndex];
    half4                               g1 = half4(normalMaterial.Load(pixel));
    float4                              surface = baseColorMetallic.Load(pixel);
    float2                              properties = unpackMaterialProperties(float2(g1.ba));
    float3                              n = decodeOctNormal(float2(g1.xy));
    half3                               baseColor = half3(surface.rgb);

    float3           worldPos = reconstructWorldPosition(input.uv, depth, frame[0].inverseViewProjection);
    int2             size;
    depthTexture.GetDimensions(size.x, size.y);
    RTUpsample upsample;
    if (rtShadowAOIndex != INVALID_TEXTURE || rtReflectionIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> traced = ResourceDescriptorHeap[rtShadowAOIndex != INVALID_TEXTURE ? rtShadowAOIndex : rtReflectionIndex];
        int2              lowSize;
        traced.GetDimensions(lowSize.x, lowSize.y);
        upsample = rtUpsample(pixel.xy, size, lowSize, length(worldPos - frame[0].eye.xyz), depthTexture, frame[0]);
    }
    LightingBindings bindings = { shadowMapIndex, shadowSamplerIndex,      environmentIndex, irradianceIndex,
                                  brdfLutIndex,   environmentSamplerIndex, rtShadowAOIndex,  rtReflectionIndex };
    float            visibility = 1.0;
    float            ao = 1.0;
    if (rtShadowAOIndex != INVALID_TEXTURE)
    {
        Texture2D<float2> traced = ResourceDescriptorHeap[rtShadowAOIndex];
        float2            rtData = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            rtData += traced.Load(int3(upsample.taps[i], 0)) * upsample.weights[i];
        }
        if (light.rtShadowEnabled != 0)
        {
            visibility = rtData.r;
        }
        else
        {
            visibility = shadowVisibility(worldPos, light, bindings);
        }
        if (light.rtAOEnabled != 0)
        {
            ao *= pow(saturate(rtData.g), light.rtAOPower);
        }
    }
    else
    {
        visibility = shadowVisibility(worldPos, light, bindings);
    }
    if (ssaoIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> occlusion = ResourceDescriptorHeap[ssaoIndex];
        ao *= occlusion.Load(pixel).r;
    }

    float3      l = -normalize(light.lightDirection.xyz);
    float3      v = normalize(frame[0].eye.xyz - worldPos);
    float       metallic = saturate(surface.a);
    float       roughness = saturate(properties.x);
    PBRLighting lighting =
        evaluatePBRLighting(float3(baseColor), metallic, roughness, n, v, l, visibility, ao, light, bindings, true, properties.y);
    if (rtReflectionIndex != INVALID_TEXTURE && roughness < kRTReflectionMaxRoughness)
    {
        Texture2D<float4> reflection = ResourceDescriptorHeap[rtReflectionIndex];
        float3            reflected = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            reflected += reflection.Load(int3(upsample.taps[i], 0)).rgb * upsample.weights[i];
        }
        lighting.indirectSpecular = lerp(lighting.specularWeight * reflected, lighting.indirectSpecular,
                                         smoothstep(kRTReflectionFadeStart, kRTReflectionMaxRoughness, roughness));
    }
    return float4(float3(scene) + lighting.direct + lighting.indirectDiffuse + lighting.indirectSpecular, 1.0);
}
