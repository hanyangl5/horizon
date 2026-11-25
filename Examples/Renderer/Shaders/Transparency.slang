#include "DeferredLightCommon.slang"
// Cookbook per-pixel linked lists / sorted transparency; see ../Cookbook-LICENSE.txt.
#include "Geometry.slang"
#include "LightingBRDF.slang"
#include "Math.slang"

[numthreads(16, 16, 1)]
void ClearMain(uint3 ID : SV_DispatchThreadID)
{
    RWTexture2D<uint> Heads = ResourceDescriptorHeap[OitHeadsIndex];
    uint              Width;
    uint              Height;
    Heads.GetDimensions(Width, Height);
    if (ID.x >= Width || ID.y >= Height)
    {
        return;
    }
    Heads[ID.xy] = 0xffffffffu;
    if (all(ID.xy == 0))
    {
        RWByteAddressBuffer Counters = ResourceDescriptorHeap[OitCounterIndex];
        Counters.Store4(0, 0);
    }
}

struct OITNode
{
    uint2 Color;
    float Depth;
    uint Next;
};

uint2 PackFragment(float3 Color, float Alpha)
{
    const float4 Value = float4(Color, Alpha);
    const uint4  Packed = f32tof16(Value);
    return uint2(Packed.x | (Packed.y << 16), Packed.z | (Packed.w << 16));
}
float4 UnpackFragment(uint2 Packed)
{
    return float4(f16tof32(Packed.x & 0xffffu), f16tof32(Packed.x >> 16), f16tof32(Packed.y & 0xffffu), f16tof32(Packed.y >> 16));
}

[earlydepthstencil] void PSTransparent(PSInput Input, bool FrontFace : SV_IsFrontFace)
{
    ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
    GpuMaterialData   Mat = Materials.Load<GpuMaterialData>(Input.MaterialData.x * sizeof(GpuMaterialData));
    if (Mat.AlphaMode != 2)
    {
        return;
    }
    SamplerState Sampler = SamplerDescriptorHeap[SamplerIndex];
    float4       EmissiveColor = float4(Mat.EmissiveFactor, 0);
    if (Mat.EmissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Texture = ResourceDescriptorHeap[NonUniformResourceIndex(Mat.EmissiveTexture)];
        EmissiveColor *= Texture.Sample(Sampler, Input.Uv);
    }
    float4 BaseColor = Mat.BaseColorFactor;
    if (Mat.BaseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Texture = ResourceDescriptorHeap[NonUniformResourceIndex(Mat.BaseColorTexture)];
        BaseColor *= Texture.Sample(Sampler, Input.Uv);
    }

    float3 N = SurfaceNormal(Input.Normal, Input.WorldPosition);
    if (Mat.NormalTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Texture = ResourceDescriptorHeap[NonUniformResourceIndex(Mat.NormalTexture)];
        float3            NormalSample = Texture.Sample(Sampler, Input.Uv).xyz;
        // Bistro uses BC5 normal maps: reconstruct Z before the reference RGB normal mapping.
        float2            NormalXY = NormalSample.xy * 2.0 - 1.0;
        NormalSample.z = 0.5 + 0.5 * sqrt(saturate(1.0 - dot(NormalXY, NormalXY)));
        if (length(NormalSample) > 0.5)
        {
            N = PerturbNormal(N, Input.WorldPosition, NormalSample, Input.Uv);
        }
    }

    StructuredBuffer<FrameData>         Frame = ResourceDescriptorHeap[FrameIndex];
    StructuredBuffer<LightingFrameData> Lighting = ResourceDescriptorHeap[LightingFrameIndex];
    LightingFrameData                   Light = Lighting[0];
    float                               NdotL = saturate(dot(N, -normalize(Light.LightDirection.xyz)));
    // Visibility comes from the shadow map: the ray traced buffer is traced from the opaque surface behind the
    // fragment, so its value describes a different point and would darken glass in front of a shadowed wall.
    LightingBindings ShadowBindings = { ShadowMapIndex,          ShadowSamplerIndex, INVALID_TEXTURE, INVALID_TEXTURE, INVALID_TEXTURE,
                                       EnvironmentSamplerIndex, INVALID_TEXTURE,    INVALID_TEXTURE };
    const float      Visibility = ShadowVisibility(Input.WorldPosition, Light, ShadowBindings);

    const float4 F0 = float4(0.04);
    float3       Sky = float3(-N.x, N.y, -N.z);
    float4       Irradiance = float4(0.0);
    float3       ColorRefl = float3(0.0);
    if (EnvironmentSamplerIndex != INVALID_TEXTURE)
    {
        SamplerState EnvironmentSampler = SamplerDescriptorHeap[EnvironmentSamplerIndex];
        if (IrradianceIndex != INVALID_TEXTURE || EnvironmentIndex != INVALID_TEXTURE)
        {
            TextureCube<float4> Texture = ResourceDescriptorHeap[IrradianceIndex != INVALID_TEXTURE ? IrradianceIndex : EnvironmentIndex];
            Irradiance = Texture.SampleLevel(EnvironmentSampler, Sky, IrradianceIndex != INVALID_TEXTURE ? 0 : Light.EnvironmentMipCount - 1);
        }
        if (EnvironmentIndex != INVALID_TEXTURE)
        {
            TextureCube<float4> Texture = ResourceDescriptorHeap[EnvironmentIndex];
            float3              V = normalize(Frame[0].Eye.xyz - Input.WorldPosition);
            float3              Reflection = reflect(V, N);
            Reflection = float3(Reflection.x, -Reflection.y, Reflection.z);
            ColorRefl = Texture.SampleLevel(EnvironmentSampler, Reflection, 0).rgb * max(Light.EnvironmentIntensity, 0.0) * Light.EnvironmentTint.rgb;
        }
    }
    float4 Diffuse = (Irradiance * max(Light.EnvironmentIntensity, 0.0) * float4(Light.EnvironmentTint.rgb, 1.0) +
                      float4(((Light.MainExcludedPasses & 1u) == 0 ? Light.MainDiffuseScale : 0.0) * NdotL * Visibility * max(Light.LightDirection.w, 0.0) * max(Light.LightColor.rgb, 0.0) / PI, 0.0)) *
                     BaseColor * (float4(1.0) - F0);

    float3 V = normalize(Frame[0].Eye.xyz - Input.WorldPosition);
    float3 KS = FresnelSchlickRoughness(clamp(dot(N, V), 0.0, 1.0), F0.rgb, 0.1);
    const FGBufferData Surface = MakeDefaultLitMaterial(N, BaseColor.rgb, Mat.MetallicFactor, Mat.RoughnessFactor, 0.5);
    const float3 LocalLighting = AccumulateScreenLights(uint2(Input.Position.xy), Frame[0].Near, Light, Surface, Input.WorldPosition, V);
    float3 Color = EmissiveColor.rgb + Diffuse.rgb + ColorRefl * KS + LocalLighting;

    // Horizon's glTF material stores the final blend alpha in baseColor.
    float Alpha = clamp(BaseColor.a, 0.0, 1.0);
    bool  IsTransparent = (Alpha > 0.01) && (Alpha < 0.99);
    if (IsTransparent && !IsHelperLane())
    {
        RWByteAddressBuffer Counters = ResourceDescriptorHeap[OitCounterIndex];
        uint                Index;
        Counters.InterlockedAdd(0, 1, Index);
        if (Index < OitMaxNodes)
        {
            RWTexture2D<uint> Heads = ResourceDescriptorHeap[OitHeadsIndex];
            uint              PrevIndex;
            InterlockedExchange(Heads[uint2(Input.Position.xy)], Index, PrevIndex);
            uint2               PackedColor = PackFragment(Color, Alpha);
            RWByteAddressBuffer Nodes = ResourceDescriptorHeap[OitNodesIndex];
            Nodes.Store4(Index * sizeof(OITNode), uint4(PackedColor, asuint(Input.Position.z), PrevIndex));
        }
        else
        {
            uint Ignored;
            Counters.InterlockedAdd(4, 1, Ignored);
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
void ResolveMain(uint3 ID : SV_DispatchThreadID)
{
    Texture2D<uint> Heads = ResourceDescriptorHeap[OitHeadsIndex];
    uint            Width;
    uint            Height;
    Heads.GetDimensions(Width, Height);
    if (ID.x >= Width || ID.y >= Height)
    {
        return;
    }
    ByteAddressBuffer   Nodes = ResourceDescriptorHeap[OitNodesIndex];
    TransparentFragment Frags[64];
    uint                NumFragments = 0;
    uint                Idx = Heads.Load(int3(ID.xy, 0));
    while (Idx != 0xffffffffu && Idx < OitMaxNodes && NumFragments < 64)
    {
        uint4 Packed = Nodes.Load4(Idx * sizeof(OITNode));
        OITNode Node = { Packed.xy, asfloat(Packed.z), Packed.w };
        Frags[NumFragments].Color = half4(UnpackFragment(Node.Color));
        Frags[NumFragments].Depth = Node.Depth;
        Frags[NumFragments].Next = Node.Next;
        NumFragments++;
        Idx = Node.Next;
    }
    if (Idx != 0xffffffffu)
    {
        RWByteAddressBuffer Counters = ResourceDescriptorHeap[OitCounterIndex];
        uint                Ignored;
        Counters.InterlockedAdd(8, 1, Ignored);
    }

    for (int I = 1; I < NumFragments; I++)
    {
        TransparentFragment ToInsert = Frags[I];
        uint                J = I;
        while (J > 0 && ToInsert.Depth > Frags[J - 1].Depth)
        {
            Frags[J] = Frags[J - 1];
            J--;
        }
        Frags[J] = ToInsert;
    }

    Texture2D<float4> Opaque = ResourceDescriptorHeap[OitOpaqueIndex];
    float4            Color = Opaque.Load(int3(ID.xy, 0));
    for (uint I = 0; I < NumFragments; I++)
    {
        Color = lerp(Color, float4(Frags[I].Color), clamp(float(Frags[I].Color.a + OitOpacityBoost), 0.0, 1.0));
    }
    if (OitShowHeatmap > 0 && NumFragments > 0)
    {
        Color = (1.0 + sin(5.0 * OitTime)) * float4(float3(NumFragments + 1, NumFragments + 1, 0), 0.0) / 16.0;
    }
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OitOutputIndex];
    Output[ID.xy] = Color;
}
