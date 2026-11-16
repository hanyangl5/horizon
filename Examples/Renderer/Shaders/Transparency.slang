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

struct OITNode
{
    uint2 Color;
    float Depth;
    uint Next;
};

uint2 PackFragment(float3 Color, float alpha)
{
    const float4 value = float4(Color, alpha);
    const uint4  packed = f32tof16(value);
    return uint2(packed.x | (packed.y << 16), packed.z | (packed.w << 16));
}
float4 UnpackFragment(uint2 packed)
{
    return float4(f16tof32(packed.x & 0xffffu), f16tof32(packed.x >> 16), f16tof32(packed.y & 0xffffu), f16tof32(packed.y >> 16));
}

[earlydepthstencil] void PSTransparent(PSInput input, bool frontFace : SV_IsFrontFace)
{
    ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
    GpuMaterialData   mat = materials.Load<GpuMaterialData>(input.MaterialData.x * sizeof(GpuMaterialData));
    if (mat.alphaMode != 2)
    {
        return;
    }
    SamplerState sampler = SamplerDescriptorHeap[samplerIndex];
    float4       emissiveColor = float4(mat.emissiveFactor, 0);
    if (mat.emissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(mat.emissiveTexture)];
        emissiveColor *= texture.Sample(sampler, input.Uv);
    }
    float4 baseColor = mat.baseColorFactor;
    if (mat.baseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(mat.baseColorTexture)];
        baseColor *= texture.Sample(sampler, input.Uv);
    }

    float3 n = normalize(input.Normal);
    if (mat.normalTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> texture = ResourceDescriptorHeap[NonUniformResourceIndex(mat.normalTexture)];
        float3            normalSample = texture.Sample(sampler, input.Uv).xyz;
        // Bistro uses BC5 normal maps: reconstruct Z before the reference RGB normal mapping.
        float2            normalXY = normalSample.xy * 2.0 - 1.0;
        normalSample.z = 0.5 + 0.5 * sqrt(saturate(1.0 - dot(normalXY, normalXY)));
        if (length(normalSample) > 0.5)
        {
            n = PerturbNormal(n, input.WorldPosition, normalSample, input.Uv);
        }
    }

    StructuredBuffer<FrameData>         frame = ResourceDescriptorHeap[frameIndex];
    StructuredBuffer<LightingFrameData> lighting = ResourceDescriptorHeap[lightingFrameIndex];
    LightingFrameData                   light = lighting[0];
    float                               NdotL = saturate(dot(n, -normalize(light.lightDirection.xyz)));
    // Visibility comes from the shadow map: the ray traced buffer is traced from the opaque surface behind the
    // fragment, so its value describes a different point and would darken glass in front of a shadowed wall.
    LightingBindings shadowBindings = { shadowMapIndex,          shadowSamplerIndex, INVALID_TEXTURE, INVALID_TEXTURE, INVALID_TEXTURE,
                                       environmentSamplerIndex, INVALID_TEXTURE,    INVALID_TEXTURE };
    const float      visibility = shadowVisibility(input.WorldPosition, light, shadowBindings);

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
            float3              v = normalize(frame[0].eye.xyz - input.WorldPosition);
            float3              reflection = reflect(v, n);
            reflection = float3(reflection.x, -reflection.y, reflection.z);
            colorRefl = texture.SampleLevel(environmentSampler, reflection, 0).rgb * max(light.environmentIntensity, 0.0);
        }
    }
    float4 diffuse = (irradiance * max(light.environmentIntensity, 0.0) +
                      float4(NdotL * visibility * max(light.lightDirection.w, 0.0) * max(light.lightColor.rgb, 0.0) / M_PI, 0.0)) *
                     baseColor * (float4(1.0) - f0);

    float3 v = normalize(frame[0].eye.xyz - input.WorldPosition);
    float3 kS = fresnelSchlickRoughness(clamp(dot(n, v), 0.0, 1.0), f0.rgb, 0.1);
    float3 Color = emissiveColor.rgb + diffuse.rgb + colorRefl * kS;

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
            InterlockedExchange(heads[uint2(input.Position.xy)], index, prevIndex);
            uint2               packedColor = PackFragment(Color, alpha);
            RWByteAddressBuffer nodes = ResourceDescriptorHeap[oitNodesIndex];
            nodes.Store4(index * sizeof(OITNode), uint4(packedColor, asuint(input.Position.z), prevIndex));
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
    half4 Color;
    float Depth;
    uint  Next;
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
        uint4 packed = nodes.Load4(idx * sizeof(OITNode));
        OITNode node = { packed.xy, asfloat(packed.z), packed.w };
        frags[numFragments].Color = half4(UnpackFragment(node.Color));
        frags[numFragments].Depth = node.Depth;
        frags[numFragments].Next = node.Next;
        numFragments++;
        idx = node.Next;
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
        while (j > 0 && toInsert.Depth > frags[j - 1].Depth)
        {
            frags[j] = frags[j - 1];
            j--;
        }
        frags[j] = toInsert;
    }

    Texture2D<float4> opaque = ResourceDescriptorHeap[oitOpaqueIndex];
    float4            Color = opaque.Load(int3(id.xy, 0));
    for (uint i = 0; i < numFragments; i++)
    {
        Color = lerp(Color, float4(frags[i].Color), clamp(float(frags[i].Color.a + oitOpacityBoost), 0.0, 1.0));
    }
    if (oitShowHeatmap > 0 && numFragments > 0)
    {
        Color = (1.0 + sin(5.0 * oitTime)) * float4(float3(numFragments + 1, numFragments + 1, 0), 0.0) / 16.0;
    }
    RWTexture2D<float4> output = ResourceDescriptorHeap[oitOutputIndex];
    output[id.xy] = Color;
}
