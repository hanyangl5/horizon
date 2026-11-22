// Screen-space lighting and RT upsampling build on the Cookbook; see ../Cookbook-LICENSE.txt.
#include "Common.slang"
#include "LightingBRDF.slang"
#include "Math.slang"
#include "DeferredLightCommon.slang"

cbuffer RootConstant0
{
    uint FrameIndex;
    uint LightingFrameIndex;
    uint EmissiveIndex;
    uint NormalMaterialIndex;
    uint BaseColorMetallicIndex;
    uint DepthIndex;
    uint ShadowMapIndex;
    uint ShadowSamplerIndex;
    uint EnvironmentIndex;
    uint IrradianceIndex;
    uint BrdfLutIndex;
    uint EnvironmentSamplerIndex;
    uint RtShadowAOIndex;
    uint RtReflectionIndex;
    uint GTAOIndex;
};
struct PSInput
{
    float4 Position: SV_Position;
    float2 Uv: TEXCOORD0;
};
PSInput VSMain(uint VertexId : SV_VertexID)
{
    const float2 UV = float2((VertexId << 1) & 2, VertexId & 2);
    PSInput      Output;
    Output.Position = float4(UV * float2(2, -2) + float2(-1, 1), 0, 1);
    Output.Uv = UV;
    return Output;
}

FGBufferData DecodeGBufferData(float4 NormalMaterial, float4 BaseColorMetallic)
{
    const float2 RoughnessSpecular = UnpackMaterialProperties(NormalMaterial.ba);

    return MakeDefaultLitMaterial(DecodeOctNormal(NormalMaterial.rg), BaseColorMetallic.rgb, BaseColorMetallic.a,
                                  RoughnessSpecular.x, RoughnessSpecular.y);
}

FGBufferData GetGBufferDataUint(uint2 PixelPos)
{
    Texture2D<float4> NormalMaterial = ResourceDescriptorHeap[NormalMaterialIndex];
    Texture2D<float4> BaseColorMetallic = ResourceDescriptorHeap[BaseColorMetallicIndex];
    const int3        Texel = int3(PixelPos, 0);
    return DecodeGBufferData(NormalMaterial.Load(Texel), BaseColorMetallic.Load(Texel));
}

// Depth-aware upsample of the reduced-resolution ray traced results: bilinear weights over the four nearest texels,
// each scaled down by its source pixel's relative view distance difference, so the results do not bleed across edges.
static const float RTUpsampleDepthScale = 50.0;
struct RTUpsample
{
    int2  Taps[4];
    float Weights[4];
};
RTUpsample UpsampleRT(int2 Pixel, int2 Size, int2 LowSize, float ViewDistance, Texture2D<float> DepthTexture, FrameData Frame)
{
    const float2 Position = float2(Pixel) * float2(LowSize) / float2(Size);
    const int2   Base = int2(floor(Position));
    const float2 Fraction = Position - float2(Base);
    RTUpsample   Result;
    float        WeightSum = 0.0;
    float        Closest = 1e30;
    int          ClosestTap = 0;
    for (int I = 0; I < 4; ++I)
    {
        const int2   Offset = int2(I & 1, I >> 1);
        const int2   Tap = min(Base + Offset, LowSize - 1);
        const int2   Source = RTSourcePixel(Tap, LowSize, Size);
        const float  TapDepth = DepthTexture.Load(int3(Source, 0));
        const float2 Bilinear = lerp(1.0 - Fraction, Fraction, float2(Offset));
        const float  Difference =
            TapDepth >= 1.0
                ? 1e30
                : abs(length(ReconstructWorldPosition((float2(Source) + 0.5) / float2(Size), TapDepth, Frame.InverseViewProjection) -
                             Frame.Eye.xyz) - ViewDistance) / ViewDistance;
        Result.Taps[I] = Tap;
        Result.Weights[I] = Bilinear.x * Bilinear.y * exp(-Difference * RTUpsampleDepthScale);
        WeightSum += Result.Weights[I];
        if (Difference < Closest)
        {
            Closest = Difference;
            ClosestTap = I;
        }
    }
    for (int I = 0; I < 4; ++I)
    {
        // Thin geometry may match none of the texels; it takes the nearest in depth instead of a blend of the background.
        Result.Weights[I] = WeightSum > 1e-4 ? Result.Weights[I] / WeightSum : float(I == ClosestTap);
    }
    return Result;
}

float4 PSMain(PSInput Input): SV_Target0
{
    const int3        Pixel = int3(int2(Input.Position.xy), 0);
    Texture2D<float4> Emissive = ResourceDescriptorHeap[EmissiveIndex];
    Texture2D<float>  DepthTexture = ResourceDescriptorHeap[DepthIndex];
    half3             Scene = half3(Emissive.Load(Pixel).rgb);
    float             Depth = DepthTexture.Load(Pixel);
    // The depth target clears to 1.0 and the projection maps the far plane there, so only the cleared
    // value means "no geometry". Anything below it is lit, however far away the far plane puts it.
    if (Depth >= 1.0)
    {
        return float4(float3(Scene), 1.0);
    }

    StructuredBuffer<FrameData>         Frame = ResourceDescriptorHeap[FrameIndex];
    StructuredBuffer<LightingFrameData> LightBuffer = ResourceDescriptorHeap[LightingFrameIndex];
    LightingFrameData                   Light = LightBuffer[0];
    const FGBufferData                  GBuffer = GetGBufferDataUint(uint2(Pixel.xy));
    const float3                        WorldPos = ReconstructWorldPosition(Input.Uv, Depth, Frame[0].InverseViewProjection);
    int2             Size;
    DepthTexture.GetDimensions(Size.x, Size.y);
    RTUpsample Upsample;
    if (RtShadowAOIndex != INVALID_TEXTURE || RtReflectionIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> Traced = ResourceDescriptorHeap[RtShadowAOIndex != INVALID_TEXTURE ? RtShadowAOIndex : RtReflectionIndex];
        int2              LowSize;
        Traced.GetDimensions(LowSize.x, LowSize.y);
        Upsample = UpsampleRT(Pixel.xy, Size, LowSize, length(WorldPos - Frame[0].Eye.xyz), DepthTexture, Frame[0]);
    }
    LightingBindings Bindings = { ShadowMapIndex, ShadowSamplerIndex,      EnvironmentIndex, IrradianceIndex,
                                  BrdfLutIndex,   EnvironmentSamplerIndex, RtShadowAOIndex,  RtReflectionIndex };
    float            Visibility = 1.0;
    float            AO = 1.0;
    if (RtShadowAOIndex != INVALID_TEXTURE)
    {
        Texture2D<float2> Traced = ResourceDescriptorHeap[RtShadowAOIndex];
        float2            RtData = 0.0;
        for (int I = 0; I < 4; ++I)
        {
            RtData += Traced.Load(int3(Upsample.Taps[I], 0)) * Upsample.Weights[I];
        }
        if (Light.RTShadowEnabled != 0)
        {
            Visibility = RtData.r;
        }
        else
        {
            Visibility = ShadowVisibility(WorldPos, Light, Bindings);
        }
        if (Light.RTAOEnabled != 0)
        {
            AO *= pow(saturate(RtData.g), Light.RTAOPower);
        }
    }
    else
    {
        Visibility = ShadowVisibility(WorldPos, Light, Bindings);
    }
    if (GTAOIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> Occlusion = ResourceDescriptorHeap[GTAOIndex];
        AO *= Occlusion.Load(Pixel).r;
    }

    const float3 V = normalize(Frame[0].Eye.xyz - WorldPos);
    FIndirectLighting Indirect = EvaluateIndirectLighting(GBuffer.DiffuseColor, GBuffer.SpecularColor, GBuffer.Roughness, GBuffer.WorldNormal, V,
                                                           AO, Light, Bindings, true);
    if (RtReflectionIndex != INVALID_TEXTURE && GBuffer.Roughness < RTReflectionMaxRoughness)
    {
        Texture2D<float4> Reflection = ResourceDescriptorHeap[RtReflectionIndex];
        float3            Reflected = 0.0;
        for (int I = 0; I < 4; ++I)
        {
            Reflected += Reflection.Load(int3(Upsample.Taps[I], 0)).rgb * Upsample.Weights[I];
        }
        Indirect.Specular = lerp(Indirect.SpecularWeight * Reflected, Indirect.Specular,
                                 smoothstep(RTReflectionFadeStart, RTReflectionMaxRoughness, GBuffer.Roughness));
    }

    const float3 DirectLighting = AccumulateSceneLights(Light, GBuffer, WorldPos, V, Visibility);
    return float4(float3(Scene) + DirectLighting + Indirect.Diffuse + Indirect.Specular, 1.0);
}
