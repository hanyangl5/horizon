#include <gtest/gtest.h>

#include "RHI/IGraphics.h"

// Verifies that default GPU settings reset the structure and enable the baseline capabilities expected by the renderer.
TEST(RHIGraphicsConfigTest, DefaultGpuSettingsInitializeBaselineCapabilities)
{
    GPUSettings settings = {};
    settings.mUniformBufferAlignment = 256;
    settings.mSamplerAnisotropySupported = 0;
    settings.mGraphicsQueueSupported = 0;
    settings.mPrimitiveIdSupported = 0;
    settings.mMaxBoundTextures = 99;
    settings.mMaxShaderModel = 0x6A;
    settings.mNative16BitShaderOpsSupported = 1;
    settings.mInt64ShaderOpsSupported = 1;
    settings.mShaderExecutionReorderingActuallyReorders = 1;
    settings.mLinearAlgebraSupported = 1;
    settings.mLinearAlgebraTier = 0x10;
    settings.mMax1DDispatchSize = 1024;
    //settings.mEnhancedBarriersSupported = 1;
    settings.mExecuteIndirectIncrementingConstantSupported = 1;

    setDefaultGPUSettings(&settings);

    EXPECT_EQ(settings.mUniformBufferAlignment, 0u);
    EXPECT_EQ(settings.mMaxBoundTextures, 0u);
    EXPECT_EQ(settings.mMaxShaderModel, 0u);
    EXPECT_EQ(settings.mNative16BitShaderOpsSupported, 0u);
    EXPECT_EQ(settings.mInt64ShaderOpsSupported, 0u);
    EXPECT_EQ(settings.mShaderExecutionReorderingActuallyReorders, 0u);
    EXPECT_EQ(settings.mLinearAlgebraSupported, 0u);
    EXPECT_EQ(settings.mLinearAlgebraTier, 0u);
    EXPECT_EQ(settings.mMax1DDispatchSize, 0u);
    EXPECT_EQ(settings.mSamplerAnisotropySupported, 1u);
    EXPECT_EQ(settings.mGraphicsQueueSupported, 1u);
    EXPECT_EQ(settings.mPrimitiveIdSupported, 1u);
    //EXPECT_EQ(settings.mEnhancedBarriersSupported, 0u);
    EXPECT_EQ(settings.mExecuteIndirectIncrementingConstantSupported, 0u);
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
