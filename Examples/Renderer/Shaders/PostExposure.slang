#pragma once

// Exposure compensation is a multiplier; metering is scene-referred and independent of it.
float ExposureScale(float AverageLuminance, float Compensation) { return max(Compensation, 0.0) * 0.18 / max(AverageLuminance, 0.0001); }

float Luminance(float3 V) { return dot(V, float3(0.2126, 0.7152, 0.0722)); }
