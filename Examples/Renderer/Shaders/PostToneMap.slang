#include "PostExposure.slang"
#include "PostColorSpace.slang"

cbuffer RootConstant0
{
    uint  SceneColorIndex;
    uint  AdaptedLuminanceIndex;
    uint  BloomIndex;
    uint  SamplerIndex;
    uint  ToneMapping;
    float Exposure;
    float BloomStrength;
    float MaxWhite;
    float P;
    float A;
    float M;
    float L;
    float C;
    float B;
    float StartCompression;
    float Desaturation;
    uint  OutputMode;
    uint  EncodeSRGB;
    float PaperWhiteNits;
    float PeakNits;
};

struct PostToneInput
{
    float4 Position: SV_Position;
    float2 Uv: TEXCOORD0;
};

[shader("vertex")]
PostToneInput VSMain(uint VertexId : SV_VertexID)
{
    float2        Uv = float2((VertexId << 1) & 2, VertexId & 2);
    PostToneInput Output;
    Output.Position = float4(Uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    Output.Uv = Uv;
    return Output;
}

float3 Reinhard2(float3 V, float MaxWhite)
{
    float L_old = Luminance(V);
    float L_new = L_old * (1.0 + (L_old / (MaxWhite * MaxWhite))) / (1.0 + L_old);
    return V * (L_new / max(L_old, 0.0001));
}

float3 Uchimura(float3 X, float P, float A, float M, float LValue, float C, float B)
{
    float L0Value = ((P - M) * LValue) / A;
    float L0 = M - M / A;
    float L1 = M + (1.0 - M) / A;
    float S0 = M + L0Value;
    float S1 = M + A * L0Value;
    float C2 = (A * P) / (P - S1);
    float CP = -C2 / P;

    float3 W0 = float3(1.0 - smoothstep(0.0, M, X));
    float3 W2 = float3(step(M + L0Value, X));
    float3 W1 = float3(1.0 - W0 - W2);

    float3 T = float3(M * pow(X / M, float3(C)) + B);
    float3 S = float3(P - (P - S1) * exp(CP * (X - S0)));
    float3 L = float3(M + A * (X - M));

    return T * W0 + L * W1 + S * W2;
}

float3 PBRNeutralToneMapping(float3 Color, float StartCompression, float Desaturation)
{
    StartCompression -= 0.04;

    float X = min(Color.r, min(Color.g, Color.b));
    float Offset = X < 0.08 ? X - 6.25 * X * X : 0.04;
    Color -= Offset;

    float Peak = max(Color.r, max(Color.g, Color.b));
    if (Peak < StartCompression)
    {
        return Color;
    }

    const float D = 1. - StartCompression;
    float       NewPeak = 1. - D * D / (Peak + D - StartCompression);
    Color *= NewPeak / Peak;

    float G = 1. - 1. / (Desaturation * (Peak - NewPeak) + 1.);
    return lerp(Color, NewPeak * float3(1, 1, 1), G);
}

[shader("fragment")]
float4 PSMain(PostToneInput Input): SV_Target0
{
    Texture2D<float4> SceneColor = ResourceDescriptorHeap[SceneColorIndex];
    Texture2D<float>  Luminance = ResourceDescriptorHeap[AdaptedLuminanceIndex];
    Texture2D<float4> Bloom = ResourceDescriptorHeap[BloomIndex];
    SamplerState      ClampSampler = SamplerDescriptorHeap[SamplerIndex];
    float3            Color = SceneColor.SampleLevel(ClampSampler, Input.Uv, 0).rgb;
    float3            BloomColor = float3(0.0);
    if (BloomStrength > 0.0)
    {
        BloomColor = Bloom.SampleLevel(ClampSampler, Input.Uv, 0).rgb;
    }
    float AvgLuminance = Luminance.Load(int3(0, 0, 0));
    float Scale = ToneMapping != 0 ? ExposureScale(AvgLuminance, Exposure) : Exposure;
    Color = Color * Scale + BloomStrength * BloomColor;

    if (ToneMapping == 1)
    {
        Color = Reinhard2(Color, MaxWhite);
    }
    if (ToneMapping == 2)
    {
        Color = Uchimura(Color, P, A, M, L, C, B);
    }
    if (ToneMapping == 3)
    {
        Color = PBRNeutralToneMapping(Color, StartCompression, Desaturation);
    }
    if (OutputMode == 0)
    {
        return float4(EncodeSRGB != 0 ? LinearToSRGB(Color) : Color, 1.0);
    }
    float3 Nits = ToneMapHDR(Color, PaperWhiteNits, PeakNits);
    if (OutputMode == 2)
    {
        return float4(Nits / 80.0, 1.0);
    }
    return float4(LinearNitsToPQ(min(Rec709ToRec2020(Nits), PeakNits)), 1.0);
}
