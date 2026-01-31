#ifndef POSTPROCESS_H
#define POSTPROCESS_H

float3 TonemapACES(float3 x) {
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    return (x * (A * x + B)) / (x * (C * x + D) + E);
}

float3 GammaCorrection(float3 x) { return float3(pow(x.r, 1.0 / 2.2), pow(x.g, 1.0 / 2.2), pow(x.b, 1.0 / 2.2)); }

#endif
