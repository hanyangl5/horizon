#include <gtest/gtest.h>

#include <string.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"
#include "RHI/IShaderReflection.h"

namespace
{
uint32_t findResourceIndexByName(const PipelineReflection& reflection, const char* name)
{
    for (uint32_t i = 0; i < reflection.shaderResourceCount; ++i)
    {
        if (strcmp(reflection.pShaderResources[i].name, name) == 0)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

uint32_t findVariableIndexByName(const PipelineReflection& reflection, const char* name)
{
    for (uint32_t i = 0; i < reflection.variableCount; ++i)
    {
        if (strcmp(reflection.pVariables[i].name, name) == 0)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

ShaderResource makeResource(DescriptorType Type, uint32_t set, uint32_t reg, ShaderStage stage, const char* name)
{
    return {
        .Type = Type,
        .set = set,
        .reg = reg,
        .Size = 1,
        .used_stages = stage,
        .name = name,
        .name_size = (uint32_t)strlen(name),
        .dim = TEXTURE_DIM_2D,
    };
}

ShaderVariable makeVariable(const char* name, uint32_t parentIndex, uint32_t offset, uint32_t Size)
{
    return {
        .name = name,
        .parent_index = parentIndex,
        .offset = offset,
        .Size = Size,
        .name_size = (uint32_t)strlen(name),
    };
}
} // namespace

// Verifies that pipeline reflection merges duplicate resources across stages and remaps deduplicated variables to the merged parent resource.
TEST(RHIShaderReflectionTest, PipelineReflectionCombinesStagesAndDeduplicatesBindings)
{
    ShaderResource vertexResources[] = {
        makeResource(DescriptorTypeUniformBuffer, 0, 0, ShaderStageVert, "FrameData"),
        makeResource(DescriptorTypeTexture, 1, 3, ShaderStageVert, "SceneTexture"),
    };
    ShaderVariable vertexVariables[] = {
        makeVariable("ViewProj", 0, 0, 64),
        makeVariable("CameraPos", 0, 64, 16),
    };

    ShaderResource fragmentResources[] = {
        makeResource(DescriptorTypeUniformBuffer, 0, 0, ShaderStageFrag, "FrameData"),
        makeResource(DescriptorTypeTexture, 1, 3, ShaderStageFrag, "SceneTexture"),
        makeResource(DescriptorTypeRwBuffer, 2, 1, ShaderStageFrag, "OutputBuffer"),
    };
    ShaderVariable fragmentVariables[] = {
        makeVariable("ViewProj", 0, 0, 64),
        makeVariable("Exposure", 0, 80, 4),
    };

    ShaderReflection stages[2] = {
        {
            .pShaderResources = vertexResources,
            .pVariables = vertexVariables,
            .shaderStage = ShaderStageVert,
            .shaderResourceCount = TF_ARRAY_COUNT(vertexResources),
            .variableCount = TF_ARRAY_COUNT(vertexVariables),
        },
        {
            .pShaderResources = fragmentResources,
            .pVariables = fragmentVariables,
            .shaderStage = ShaderStageFrag,
            .shaderResourceCount = TF_ARRAY_COUNT(fragmentResources),
            .variableCount = TF_ARRAY_COUNT(fragmentVariables),
        },
    };

    PipelineReflection pipeline = {};
    createPipelineReflection(stages, TF_ARRAY_COUNT(stages), &pipeline);

    ASSERT_EQ(pipeline.stageReflectionCount, 2u);
    EXPECT_EQ(pipeline.shaderStages, ShaderStageVert | ShaderStageFrag);
    EXPECT_EQ(pipeline.vertexStageIndex, 0u);
    EXPECT_EQ(pipeline.pixelStageIndex, 1u);
    EXPECT_EQ(pipeline.hullStageIndex, UINT32_MAX);
    EXPECT_EQ(pipeline.domainStageIndex, UINT32_MAX);
    EXPECT_EQ(pipeline.geometryStageIndex, UINT32_MAX);

    ASSERT_EQ(pipeline.shaderResourceCount, 3u);
    const uint32_t frameDataIndex = findResourceIndexByName(pipeline, "FrameData");
    const uint32_t sceneTextureIndex = findResourceIndexByName(pipeline, "SceneTexture");
    const uint32_t outputBufferIndex = findResourceIndexByName(pipeline, "OutputBuffer");
    ASSERT_NE(frameDataIndex, UINT32_MAX);
    ASSERT_NE(sceneTextureIndex, UINT32_MAX);
    ASSERT_NE(outputBufferIndex, UINT32_MAX);

    EXPECT_EQ(pipeline.pShaderResources[frameDataIndex].used_stages, ShaderStageVert | ShaderStageFrag);
    EXPECT_EQ(pipeline.pShaderResources[sceneTextureIndex].used_stages, ShaderStageVert | ShaderStageFrag);
    EXPECT_EQ(pipeline.pShaderResources[outputBufferIndex].used_stages, ShaderStageFrag);

    ASSERT_EQ(pipeline.variableCount, 3u);
    const uint32_t viewProjIndex = findVariableIndexByName(pipeline, "ViewProj");
    const uint32_t cameraPosIndex = findVariableIndexByName(pipeline, "CameraPos");
    const uint32_t exposureIndex = findVariableIndexByName(pipeline, "Exposure");
    ASSERT_NE(viewProjIndex, UINT32_MAX);
    ASSERT_NE(cameraPosIndex, UINT32_MAX);
    ASSERT_NE(exposureIndex, UINT32_MAX);

    EXPECT_EQ(pipeline.pVariables[viewProjIndex].parent_index, frameDataIndex);
    EXPECT_EQ(pipeline.pVariables[cameraPosIndex].parent_index, frameDataIndex);
    EXPECT_EQ(pipeline.pVariables[exposureIndex].parent_index, frameDataIndex);

    MemDeallocate(pipeline.pShaderResources);
    MemDeallocate(pipeline.pVariables);
}

// Verifies that descriptor-name helpers detect root constants and root CBVs regardless of letter case.
TEST(RHIShaderReflectionTest, DescriptorNameHelpersRecognizeRootDescriptorsCaseInsensitively)
{
    EXPECT_TRUE(isDescriptorRootConstant("PerFrameRootConstant"));
    EXPECT_TRUE(isDescriptorRootConstant("material_pushconstant_data"));
    EXPECT_FALSE(isDescriptorRootConstant("SceneBuffer"));

}
