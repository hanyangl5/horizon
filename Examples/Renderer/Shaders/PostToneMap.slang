#include "PostExposure.slang"
#include "PostColorSpace.slang"

cbuffer RootConstant0
{
    uint  sceneColorIndex;
    uint  adaptedLuminanceIndex;
    uint  bloomIndex;
    uint  samplerIndex;
    uint  toneMapping;
    float exposure;
    float bloomStrength;
    float maxWhite;
    float P;
    float a;
    float m;
    float l;
    float c;
    float b;
    float startCompression;
    float desaturation;
    uint  outputMode;
    uint  encodeSRGB;
    float paperWhiteNits;
    float peakNits;
};

struct PostToneInput
{
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};

[shader("vertex")]
PostToneInput VSMain(uint vertexId : SV_VertexID)
{
    float2        uv = float2((vertexId << 1) & 2, vertexId & 2);
    PostToneInput output;
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}

float3 reinhard2(float3 v, float maxWhite)
{
    float l_old = luminance(v);
    float l_new = l_old * (1.0 + (l_old / (maxWhite * maxWhite))) / (1.0 + l_old);
    return v * (l_new / max(l_old, 0.0001));
}

float3 uchimura(float3 x, float P, float a, float m, float l, float c, float b)
{
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    float3 w0 = float3(1.0 - smoothstep(0.0, m, x));
    float3 w2 = float3(step(m + l0, x));
    float3 w1 = float3(1.0 - w0 - w2);

    float3 T = float3(m * pow(x / m, float3(c)) + b);
    float3 S = float3(P - (P - S1) * exp(CP * (x - S0)));
    float3 L = float3(m + a * (x - m));

    return T * w0 + L * w1 + S * w2;
}

float3 PBRNeutralToneMapping(float3 color, float startCompression, float desaturation)
{
    startCompression -= 0.04;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
    {
        return color;
    }

    const float d = 1. - startCompression;
    float       newPeak = 1. - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
    return lerp(color, newPeak * float3(1, 1, 1), g);
}

[shader("fragment")]
float4 PSMain(PostToneInput input): SV_Target0
{
    Texture2D<float4> sceneColor = ResourceDescriptorHeap[sceneColorIndex];
    Texture2D<float>  luminance = ResourceDescriptorHeap[adaptedLuminanceIndex];
    Texture2D<float4> bloom = ResourceDescriptorHeap[bloomIndex];
    SamplerState      clampSampler = SamplerDescriptorHeap[samplerIndex];
    float3            color = sceneColor.SampleLevel(clampSampler, input.uv, 0).rgb;
    float3            bloomColor = float3(0.0);
    if (bloomStrength > 0.0)
    {
        bloomColor = bloom.SampleLevel(clampSampler, input.uv, 0).rgb;
    }
    float avgLuminance = luminance.Load(int3(0, 0, 0));
    float scale = toneMapping != 0 ? exposureScale(avgLuminance, exposure) : exposure;
    color = color * scale + bloomStrength * bloomColor;

    if (toneMapping == 1)
    {
        color = reinhard2(color, maxWhite);
    }
    if (toneMapping == 2)
    {
        color = uchimura(color, P, a, m, l, c, b);
    }
    if (toneMapping == 3)
    {
        color = PBRNeutralToneMapping(color, startCompression, desaturation);
    }
    if (outputMode == 0)
    {
        return float4(encodeSRGB != 0 ? linearToSRGB(color) : color, 1.0);
    }
    float3 nits = toneMapHDR(color, paperWhiteNits, peakNits);
    if (outputMode == 2)
    {
        return float4(nits / 80.0, 1.0);
    }
    return float4(linearNitsToPQ(min(rec709ToRec2020(nits), peakNits)), 1.0);
}
