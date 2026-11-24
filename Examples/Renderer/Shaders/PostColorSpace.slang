#pragma once

float3 LinearToSRGB(float3 Value)
{
    float3 Color = max(Value, 0.0);
    float3 Low = Color * 12.92;
    float3 High = 1.055 * pow(Color, float3(1.0 / 2.4)) - 0.055;
    return float3(Color.r <= 0.0031308 ? Low.r : High.r, Color.g <= 0.0031308 ? Low.g : High.g, Color.b <= 0.0031308 ? Low.b : High.b);
}

float3 Rec709ToRec2020(float3 Color)
{
    const float3x3 Conversion = float3x3(0.6274040, 0.3292820, 0.0433136, 0.0690970, 0.9195400, 0.0113612, 0.0163916, 0.0880132, 0.8955950);
    return mul(Conversion, Color);
}

float3 LinearNitsToPQ(float3 Color)
{
    const float M1 = 0.1593017578125;
    const float M2 = 78.84375;
    const float C1 = 0.8359375;
    const float C2 = 18.8515625;
    const float C3 = 18.6875;
    float3      Normalized = pow(saturate(Color / 10000.0), M1);
    return pow((C1 + C2 * Normalized) / (1.0 + C3 * Normalized), M2);
}

float3 ToneMapHDR(float3 Color, float PaperWhiteNits, float PeakNits)
{
    Color = max(Color, 0.0);
    float Luminance = dot(Color, float3(0.2126, 0.7152, 0.0722));
    float PeakRatio = max(PeakNits / PaperWhiteNits, 1.0001);
    float Mapped = Luminance;
    if (Luminance > 1.0)
    {
        Mapped = 1.0 + (PeakRatio - 1.0) * (1.0 - exp(-(Luminance - 1.0) / (PeakRatio - 1.0)));
    }
    return Color * (Mapped / max(Luminance, 1e-6)) * PaperWhiteNits;
}
