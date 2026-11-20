// Adapted from 3D Graphics Rendering Cookbook, Second Edition (Packt, 2024).
// See ../Cookbook-LICENSE.txt. Uses the original rotation texels, eight samples and depth-aware 17-tap blur.
#include "Common.slang"

cbuffer RootConstant0
{
    uint  FrameIndex;
    uint  DepthIndex;
    uint  RotationsIndex;
    uint  SourceIndex;
    uint  OutputIndex;
    uint  SamplerIndex;
    uint  RotationSamplerIndex;
    uint  Horizontal;
    float Radius;
    float AttenuationScale;
    float DistanceScale;
    float DepthThreshold;
};

static const float3 Offsets[8] = { float3(-0.5, -0.5, -0.5), float3(0.5, -0.5, -0.5), float3(-0.5, 0.5, -0.5), float3(0.5, 0.5, -0.5),
                                   float3(-0.5, -0.5, 0.5),  float3(0.5, -0.5, 0.5),  float3(-0.5, 0.5, 0.5),  float3(0.5, 0.5, 0.5) };

float ScaleZ(float Smpl, float2 InverseProjection)
{
    // For an affine camera pose, inverse(VP)'s last row is inverse(P)'s last row.
    // Preserve the Cookbook's negative view-space Z without hard-coding the camera's near/far planes.
    return -rcp(Smpl * InverseProjection.x + InverseProjection.y);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 DispatchId : SV_DispatchThreadID)
{
    Texture2D<float>            DepthTexture = ResourceDescriptorHeap[DepthIndex];
    Texture2D<float4>           Rotations = ResourceDescriptorHeap[RotationsIndex];
    RWTexture2D<float4>         Output = ResourceDescriptorHeap[OutputIndex];
    SamplerState                Sampler = SamplerDescriptorHeap[SamplerIndex];
    SamplerState                NoiseSampler = SamplerDescriptorHeap[RotationSamplerIndex];
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[FrameIndex];
    float2                      InverseProjection = float2(Frame[0].InverseViewProjection[3][2], Frame[0].InverseViewProjection[3][3]);
    uint                        Width;
    uint                        Height;
    DepthTexture.GetDimensions(Width, Height);

    const float2 Size = float2(Width, Height);

    const float2 Xy = DispatchId.xy;
    const float2 UV = (DispatchId.xy + float2(0.5)) / Size;

    if (Xy.x >= Size.x || Xy.y >= Size.y)
    {
        return;
    }

    const float  Z = ScaleZ(DepthTexture.SampleLevel(Sampler, UV, 0), InverseProjection);
    const float3 Plane = Rotations.SampleLevel(NoiseSampler, Xy / 4.0, 0).xyz - float3(1.0);

    float Att = 0.0;

    for (int I = 0; I < 8; I++)
    {
        float3 RSample = reflect(Offsets[I], Plane);
        float  ZSample = ScaleZ(DepthTexture.SampleLevel(Sampler, UV + Radius * RSample.xy / Z, 0), InverseProjection);
        float  Dist = max(ZSample - Z, 0.0) / DistanceScale;
        float  Occl = 15.0 * max(Dist * (2.0 - Dist), 0.0);
        Att += 1.0 / (1.0 + Occl * Occl);
    }

    Att = clamp(Att * Att / 64.0 + 0.45, 0.0, 1.0) * AttenuationScale;

    Output[int2(Xy)] = (float4(float3(Att), 1.0));
}

static const float GaussWeights[17] = {
    0.00001525878906,
    0.0002441406250,
    0.001831054688,
    0.008544921875,
    0.02777099609,
    0.06665039063,
    0.1221923828,
    0.1745605469,
    0.1963806152,
    0.1745605469,
    0.1221923828,
    0.06665039063,
    0.02777099609,
    0.008544921875,
    0.001831054688,
    0.0002441406250,
    0.00001525878906
};

[numthreads(16, 16, 1)]
void BlurMain(uint3 DispatchId : SV_DispatchThreadID)
{
    Texture2D<float4>   Source = ResourceDescriptorHeap[SourceIndex];
    Texture2D<float>    DepthTexture = ResourceDescriptorHeap[DepthIndex];
    RWTexture2D<float4> Output = ResourceDescriptorHeap[OutputIndex];
    SamplerState        Sampler = SamplerDescriptorHeap[SamplerIndex];
    uint                Width;
    uint                Height;
    Source.GetDimensions(Width, Height);

    const float2 Size = float2(Width, Height);
    const float2 Xy = DispatchId.xy;

    if (Xy.x >= Size.x || Xy.y >= Size.y)
    {
        return;
    }

    const float2 TexCoord = (DispatchId.xy + float2(0.5)) / Size;

    const float TexScaler = 1.0 / ((Horizontal != 0) ? Size.x : Size.y);

    float3 C = float3(0.0);

    float3 FragColor = Source.SampleLevel(Sampler, TexCoord, 0).rgb;
    float  FragDepth = DepthTexture.SampleLevel(Sampler, TexCoord, 0);

    for (int I = 0; I != 17; I++)
    {
        float  Offset = float(I - 17 / 2);
        float2 UV = TexCoord + TexScaler * ((Horizontal != 0) ? float2(Offset, 0) : float2(0, Offset));
        float3 Color = Source.SampleLevel(Sampler, UV, 0).rgb;
        float  Depth = DepthTexture.SampleLevel(Sampler, UV, 0);

        float Weight = clamp(abs(Depth - FragDepth) * DepthThreshold, 0.0, 1.0);
        C += lerp(Color, FragColor, Weight) * GaussWeights[I];
    }

    Output[int2(Xy)] = (float4(C, 1.0));
}
