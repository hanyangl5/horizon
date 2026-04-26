#include <gtest/gtest.h>

#include "RHI/IGraphics.h"

// Verifies that default GPU settings reset the structure and enable the baseline capabilities expected by the renderer.
TEST(RHIGraphicsConfigTest, DefaultGpuSettingsInitializeBaselineCapabilities)
{
    GPUSettings settings = {
        .mUniformBufferAlignment = 256,
        .mPrimitiveIdSupported = 0,
        .mMaxBoundTextures = 99,
        .mSamplerAnisotropySupported = 0,
        .mGraphicsQueueSupported = 0,
    };

    setDefaultGPUSettings(&settings);

    EXPECT_EQ(settings.mUniformBufferAlignment, 0u);
    EXPECT_EQ(settings.mMaxBoundTextures, 0u);
    EXPECT_EQ(settings.mSamplerAnisotropySupported, 1u);
    EXPECT_EQ(settings.mGraphicsQueueSupported, 1u);
    EXPECT_EQ(settings.mPrimitiveIdSupported, 1u);
}

// Verifies that preset-level helpers convert between enum values and case-insensitive strings.
TEST(RHIGraphicsConfigTest, PresetLevelStringHelpersRoundTripKnownValues)
{
    EXPECT_STREQ(presetLevelToString(GPU_PRESET_HIGH), "high");
    EXPECT_EQ(stringToPresetLevel("office"), GPU_PRESET_OFFICE);
    EXPECT_EQ(stringToPresetLevel("Medium"), GPU_PRESET_MEDIUM);
    EXPECT_EQ(stringToPresetLevel("ULTRA"), GPU_PRESET_ULTRA);
    EXPECT_EQ(stringToPresetLevel("unknown"), GPU_PRESET_NONE);
}
