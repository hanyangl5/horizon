#pragma once

float3 linearToSRGB(float3 value)
{
    float3 color = max(value, 0.0);
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(color, float3(1.0 / 2.4)) - 0.055;
    return float3(color.r <= 0.0031308 ? low.r : high.r, color.g <= 0.0031308 ? low.g : high.g, color.b <= 0.0031308 ? low.b : high.b);
}

float3 rec709ToRec2020(float3 color)
{
    const float3x3 conversion = float3x3(0.6274040, 0.3292820, 0.0433136, 0.0690970, 0.9195400, 0.0113612, 0.0163916, 0.0880132, 0.8955950);
    return mul(conversion, color);
}

float3 linearNitsToPQ(float3 color)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    float3      normalized = pow(saturate(color / 10000.0), m1);
    return pow((c1 + c2 * normalized) / (1.0 + c3 * normalized), m2);
}

float3 toneMapHDR(float3 color, float paperWhiteNits, float peakNits)
{
    color = max(color, 0.0);
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float peakRatio = max(peakNits / paperWhiteNits, 1.0001);
    float mapped = luminance;
    if (luminance > 1.0)
    {
        mapped = 1.0 + (peakRatio - 1.0) * (1.0 - exp(-(luminance - 1.0) / (peakRatio - 1.0)));
    }
    return color * (mapped / max(luminance, 1e-6)) * paperWhiteNits;
}
