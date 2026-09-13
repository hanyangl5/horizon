#include <gtest/gtest.h>

#include "RHI/IGraphics.h"

// Verifies that default GPU settings reset the structure and enable the baseline capabilities expected by the renderer.
TEST(RHIGraphicsConfigTest, DefaultGpuSettingsInitializeBaselineCapabilities)
{
    GPUSettings settings = {};
    settings.uniformBufferAlignment = 256;
    settings.samplerAnisotropySupported = 0;
    settings.graphicsQueueSupported = 0;
    settings.primitiveIdSupported = 0;
    settings.maxBoundTextures = 99;
    settings.maxShaderModel = 0x6A;
    settings.native16BitShaderOpsSupported = 1;
    settings.int64ShaderOpsSupported = 1;
    settings.shaderExecutionReorderingActuallyReorders = 1;
    settings.linearAlgebraSupported = 1;
    settings.linearAlgebraTier = 0x10;
    settings.max1DDispatchSize = 1024;
    //settings.enhancedBarriersSupported = 1;
    settings.executeIndirectIncrementingConstantSupported = 1;

    setDefaultGPUSettings(&settings);

    EXPECT_EQ(settings.uniformBufferAlignment, 0u);
    EXPECT_EQ(settings.maxBoundTextures, 0u);
    EXPECT_EQ(settings.maxShaderModel, 0u);
    EXPECT_EQ(settings.native16BitShaderOpsSupported, 0u);
    EXPECT_EQ(settings.int64ShaderOpsSupported, 0u);
    EXPECT_EQ(settings.shaderExecutionReorderingActuallyReorders, 0u);
    EXPECT_EQ(settings.linearAlgebraSupported, 0u);
    EXPECT_EQ(settings.linearAlgebraTier, 0u);
    EXPECT_EQ(settings.max1DDispatchSize, 0u);
    EXPECT_EQ(settings.samplerAnisotropySupported, 1u);
    EXPECT_EQ(settings.graphicsQueueSupported, 1u);
    EXPECT_EQ(settings.primitiveIdSupported, 1u);
    //EXPECT_EQ(settings.enhancedBarriersSupported, 0u);
    EXPECT_EQ(settings.executeIndirectIncrementingConstantSupported, 0u);
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
