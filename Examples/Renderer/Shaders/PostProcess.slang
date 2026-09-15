#include "Common.slang"

cbuffer RootConstant0
{
    uint sceneColorIndex;
    uint depthIndex;
    uint outputMode;
    float paperWhiteNits;
    float peakNits;
};

static const uint OUTPUT_MODE_SDR = 0;
static const uint OUTPUT_MODE_SCRGB = 2;

struct PSInput
{
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};

PSInput VSMain(uint vertexId : SV_VertexID)
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    PSInput output;
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}

float3 toneMapHDR(float3 color)
{
    color = max(color, 0.0);
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float peakRatio = max(peakNits / paperWhiteNits, 1.0001);
    float mappedLuminance = luminance;
    if (luminance > 1.0)
        mappedLuminance = 1.0 + (peakRatio - 1.0) * (1.0 - exp(-(luminance - 1.0) / (peakRatio - 1.0)));
    return color * (mappedLuminance / max(luminance, 1e-6)) * paperWhiteNits;
}

float3 rec709ToRec2020(float3 color)
{
    const float3x3 conversion = float3x3(
        0.6274040, 0.3292820, 0.0433136,
        0.0690970, 0.9195400, 0.0113612,
        0.0163916, 0.0880132, 0.8955950);
    return mul(conversion, color);
}

float3 linearNitsToPQ(float3 color)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    float3 normalized = pow(saturate(color / 10000.0), m1);
    return pow((c1 + c2 * normalized) / (1.0 + c3 * normalized), m2);
}

float3 toneMapSDR(float3 color)
{
    color = max(color, 0.0);
    return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float4 PSMain(PSInput input): SV_Target0
{
    Texture2D<float4>           sceneColor = ResourceDescriptorHeap[sceneColorIndex];
    Texture2D<float>            SceneDepth = ResourceDescriptorHeap[depthIndex];
    int2                        pixel = int2(input.position.xy);
    float                       depth = SceneDepth.Load(int3(pixel, 0));
    if (depth >= 1.0)
    {
        if (outputMode == OUTPUT_MODE_SDR)
            return float4(0.02, 0.035, 0.055, 1.0);
        if (outputMode == OUTPUT_MODE_SCRGB)
            return float4(float3(0.02, 0.035, 0.055) * paperWhiteNits / 80.0, 1.0);
        return float4(linearNitsToPQ(rec709ToRec2020(float3(0.02, 0.035, 0.055) * paperWhiteNits)), 1.0);
    }

    float3 color = sceneColor.Load(int3(pixel, 0)).rgb;
    if (outputMode == OUTPUT_MODE_SDR)
        return float4(toneMapSDR(color), 1.0);
    if (outputMode == OUTPUT_MODE_SCRGB)
        return float4(toneMapHDR(color) / 80.0, 1.0);
    float3 color2020Nits = min(rec709ToRec2020(toneMapHDR(color)), peakNits);
    return float4(linearNitsToPQ(color2020Nits), 1.0);
}
