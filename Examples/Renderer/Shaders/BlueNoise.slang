// Spatiotemporal lookup adapted from Epic's BlueNoiseCommon.ush.
#pragma once

float2 GetBlueNoiseVec2(Texture2D<float2> Texture, uint2 ScreenCoord, uint FrameIndex)
{
    // 64 vertically stacked 128x128 slices. Masking wraps the pixel and frame coordinates.
    const uint3 Wrapped = uint3(ScreenCoord, FrameIndex) & uint3(127, 127, 63);
    return Texture.Load(int3(Wrapped.x, Wrapped.z * 128u + Wrapped.y, 0));
}

float GetBlueNoiseScalar(Texture2D<float> InTexture, int3 InDimensions, int3 InModuloMasks, uint2 InScreenCoord, uint InFrameIndex)
{
    uint3 WrappedCoordinate = uint3(InScreenCoord, InFrameIndex) & InModuloMasks;
    uint3 TextureCoordinate = uint3(WrappedCoordinate.x, WrappedCoordinate.z * InDimensions.y + WrappedCoordinate.y, 0);
    return InTexture.Load(int3(TextureCoordinate)).x;
}

float2 GetBlueNoiseVec2(Texture2D<float2> InTexture, int3 InDimensions, int3 InModuloMasks, uint2 InScreenCoord, uint InFrameIndex)
{
    uint3 WrappedCoordinate = uint3(InScreenCoord, InFrameIndex) & InModuloMasks;
    uint3 TextureCoordinate = uint3(WrappedCoordinate.x, WrappedCoordinate.z * InDimensions.y + WrappedCoordinate.y, 0);
    return InTexture.Load(int3(TextureCoordinate)).xy;
}

float GetBlueNoiseDitheredCoverage(Texture2D<float> InTexture, int3 InDimensions, int3 InModuloMasks, uint2 InScreenCoord, uint InFrameIndex, float InCoverage)
{
    // New dithering based on Blue noise
    const float Noise2 = GetBlueNoiseScalar(InTexture, InDimensions, InModuloMasks, InScreenCoord, InFrameIndex);
    const float LegacyAdjustement = 0.83f; // This is required to get the same look and empty areas as the legacy one.
    float Dither = LegacyAdjustement * Noise2;
    return InCoverage + Dither - 0.5f;
}
