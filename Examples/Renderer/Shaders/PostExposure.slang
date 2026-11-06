#pragma once

// Exposure compensation is a multiplier; metering is scene-referred and independent of it.
float exposureScale(float averageLuminance, float compensation) { return max(compensation, 0.0) * 0.18 / max(averageLuminance, 0.0001); }

float luminance(float3 v) { return dot(v, float3(0.2126, 0.7152, 0.0722)); }
