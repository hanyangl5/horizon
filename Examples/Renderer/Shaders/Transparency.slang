// Cookbook per-pixel linked lists / sorted transparency; see ../Cookbook-LICENSE.txt.
#include "Geometry.slang"
#include "LightingBRDF.slang"
#include "Math.slang"

[numthreads(16, 16, 1)]
void ClearMain(uint3 id : SV_DispatchThreadID)
{
    RWTexture2D<uint> heads = ResourceDescriptorHeap[oitHeadsIndex];
    uint              width;
    uint              height;
    heads.GetDimensions(width, height);
    if (id.x >= width || id.y >= height)
    {
        return;
    }
    heads[id.xy] = 0xffffffffu;
    if (all(id.xy == 0))
    {
        RWByteAddressBuffer counters = ResourceDescriptorHeap[oitCounterIndex];
        counters.Store4(0, 0);
    }
}

uint2 packFragment(float3 color, float alpha)
{
    const float4 value = float4(color, alpha);
    const uint4  packed = f32tof16(value);
    return uint2(packed.x | (packed.y << 16), packed.z | (packed.w << 16));
}
float4 unpackFragment(uint2 packed)
{
    return float4(f16tof32(packed.x & 0xffffu), f16tof32(packed.x >> 16), f16tof32(packed.y & 0xffffu), f16tof32(packed.y >> 16));
}

[earlydepthstencil]
void PSTransparent(PSInput input, bool frontFace : SV_IsFrontFace)
{
    Material mat = loadMaterial(input.materialData.x);
    if (mat.alphaMode != 2)
    {
        return;
    }
    SamplerState sampler = SamplerDescriptorHeap[samplerIndex];
    float4       emissiveColor = float4(mat.emissive, 0);
    if (mat.emissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(mat.emissiveTexture)];
        emissiveColor *= texture.Sample(sampler, input.uv);
    }
    float4 baseColor = mat.baseColor;
    if (mat.baseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(mat.baseColorTexture)];
        baseColor *= texture.Sample(sampler, input.uv);
    }

    float3 n = normalize(input.normal);
    if (mat.normalTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(mat.normalTexture)];
        float3            normalSample = texture.Sample(sampler, input.uv).xyz;
        // Bistro uses BC5 normal maps: reconstruct Z before the reference RGB normal mapping.
        float2            normalXY = normalSample.xy * 2.0 - 1.0;
        normalSample.z = 0.5 + 0.5 * sqrt(saturate(1.0 - dot(normalXY, normalXY)));
        if (length(normalSample) > 0.5)
        {
            n = perturbNormal(n, input.worldPosition, normalSample, input.uv);
        }
    }

    StructuredBuffer<FrameData>         frame = ResourceDescriptorHeap[frameIndex];
    StructuredBuffer<LightingFrameData> lighting = ResourceDescriptorHeap[lightingFrameIndex];
    LightingFrameData                   light = lighting[0];
    float                               NdotL = saturate(dot(n, -normalize(light.lightDirection.xyz)));

    const float4 f0 = float4(0.04);
    float3       sky = float3(-n.x, n.y, -n.z);
    float4       irradiance = float4(0.0);
    float3       colorRefl = float3(0.0);
    if (environmentSamplerIndex != INVALID_TEXTURE)
    {
        SamplerState environmentSampler = SamplerDescriptorHeap[environmentSamplerIndex];
        if (irradianceIndex != INVALID_TEXTURE)
        {
            TextureCube<float4> texture = ResourceDescriptorHeap[irradianceIndex];
            irradiance = texture.SampleLevel(environmentSampler, sky, 0);
        }
        if (environmentIndex != INVALID_TEXTURE)
        {
            TextureCube<float4> texture = ResourceDescriptorHeap[environmentIndex];
            float3              v = normalize(frame[0].eye.xyz - input.worldPosition);
            float3              reflection = reflect(v, n);
            reflection = float3(reflection.x, -reflection.y, reflection.z);
            colorRefl = texture.SampleLevel(environmentSampler, reflection, 0).rgb * max(light.environmentIntensity, 0.0);
        }
    }
    float4 diffuse = (irradiance * max(light.environmentIntensity, 0.0) +
                      float4(NdotL * max(light.lightDirection.w, 0.0) * max(light.lightColor.rgb, 0.0) / M_PI, 0.0)) *
                     baseColor * (float4(1.0) - f0);

    float3 v = normalize(frame[0].eye.xyz - input.worldPosition);
    float3 kS = fresnelSchlickRoughness(clamp(dot(n, v), 0.0, 1.0), f0.rgb, 0.1);
    float3 color = emissiveColor.rgb + diffuse.rgb + colorRefl * kS;

    // Horizon's glTF material stores the final blend alpha in baseColor.
    float alpha = clamp(baseColor.a, 0.0, 1.0);
    bool  isTransparent = (alpha > 0.01) && (alpha < 0.99);
    if (isTransparent && !IsHelperLane())
    {
        RWByteAddressBuffer counters = ResourceDescriptorHeap[oitCounterIndex];
        uint                index;
        counters.InterlockedAdd(0, 1, index);
        if (index < oitMaxNodes)
        {
            RWTexture2D<uint> heads = ResourceDescriptorHeap[oitHeadsIndex];
            uint              prevIndex;
            InterlockedExchange(heads[uint2(input.position.xy)], index, prevIndex);
            uint2               packedColor = packFragment(color, alpha);
            RWByteAddressBuffer nodes = ResourceDescriptorHeap[oitNodesIndex];
            nodes.Store4(index * 16, uint4(packedColor, asuint(input.position.z), prevIndex));
        }
        else
        {
            uint ignored;
            counters.InterlockedAdd(4, 1, ignored);
        }
    }
}

struct TransparentFragment
{
    half4 color;
    float depth;
    uint  next;
};

[numthreads(8, 8, 1)]
void ResolveMain(uint3 id : SV_DispatchThreadID)
{
    Texture2D<uint> heads = ResourceDescriptorHeap[oitHeadsIndex];
    uint            width;
    uint            height;
    heads.GetDimensions(width, height);
    if (id.x >= width || id.y >= height)
    {
        return;
    }
    ByteAddressBuffer   nodes = ResourceDescriptorHeap[oitNodesIndex];
    TransparentFragment frags[64];
    uint                numFragments = 0;
    uint                idx = heads.Load(int3(id.xy, 0));
    while (idx != 0xffffffffu && idx < oitMaxNodes && numFragments < 64)
    {
        uint4 node = nodes.Load4(idx * 16);
        frags[numFragments].color = half4(unpackFragment(node.xy));
        frags[numFragments].depth = asfloat(node.z);
        frags[numFragments].next = node.w;
        numFragments++;
        idx = node.w;
    }
    if (idx != 0xffffffffu)
    {
        RWByteAddressBuffer counters = ResourceDescriptorHeap[oitCounterIndex];
        uint                ignored;
        counters.InterlockedAdd(8, 1, ignored);
    }

    for (int i = 1; i < numFragments; i++)
    {
        TransparentFragment toInsert = frags[i];
        uint                j = i;
        while (j > 0 && toInsert.depth > frags[j - 1].depth)
        {
            frags[j] = frags[j - 1];
            j--;
        }
        frags[j] = toInsert;
    }

    Texture2D<float4> opaque = ResourceDescriptorHeap[oitOpaqueIndex];
    float4            color = opaque.Load(int3(id.xy, 0));
    for (uint i = 0; i < numFragments; i++)
    {
        color = lerp(color, float4(frags[i].color), clamp(float(frags[i].color.a + oitOpacityBoost), 0.0, 1.0));
    }
    if (oitShowHeatmap > 0 && numFragments > 0)
    {
        color = (1.0 + sin(5.0 * oitTime)) * float4(float3(numFragments + 1, numFragments + 1, 0), 0.0) / 16.0;
    }
    RWTexture2D<float4> output = ResourceDescriptorHeap[oitOutputIndex];
    output[id.xy] = color;
}
