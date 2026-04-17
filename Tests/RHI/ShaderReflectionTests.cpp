#include <gtest/gtest.h>

#include <string.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"
#include "RHI/IShaderReflection.h"

namespace
{
uint32_t findResourceIndexByName(const PipelineReflection& reflection, const char* name)
{
    for (uint32_t i = 0; i < reflection.mShaderResourceCount; ++i)
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
    for (uint32_t i = 0; i < reflection.mVariableCount; ++i)
    {
        if (strcmp(reflection.pVariables[i].name, name) == 0)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

ShaderResource makeResource(DescriptorType type, uint32_t set, uint32_t reg, ShaderStage stage, const char* name)
{
    ShaderResource resource = {};
    resource.type = type;
    resource.set = set;
    resource.reg = reg;
    resource.size = 1;
    resource.used_stages = stage;
    resource.name = name;
    resource.name_size = (uint32_t)strlen(name);
    resource.dim = TEXTURE_DIM_2D;
    return resource;
}

ShaderVariable makeVariable(const char* name, uint32_t parentIndex, uint32_t offset, uint32_t size)
{
    ShaderVariable variable = {};
    variable.name = name;
    variable.name_size = (uint32_t)strlen(name);
    variable.parent_index = parentIndex;
    variable.offset = offset;
    variable.size = size;
    return variable;
}
} // namespace

// Verifies that pipeline reflection merges duplicate resources across stages and remaps deduplicated variables to the merged parent resource.
TEST(RHIShaderReflectionTest, PipelineReflectionCombinesStagesAndDeduplicatesBindings)
{
    ShaderResource vertexResources[] = {
        makeResource(DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 0, SHADER_STAGE_VERT, "FrameData"),
        makeResource(DESCRIPTOR_TYPE_TEXTURE, 1, 3, SHADER_STAGE_VERT, "SceneTexture"),
    };
    ShaderVariable vertexVariables[] = {
        makeVariable("ViewProj", 0, 0, 64),
        makeVariable("CameraPos", 0, 64, 16),
    };

    ShaderResource fragmentResources[] = {
        makeResource(DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 0, SHADER_STAGE_FRAG, "FrameData"),
        makeResource(DESCRIPTOR_TYPE_TEXTURE, 1, 3, SHADER_STAGE_FRAG, "SceneTexture"),
        makeResource(DESCRIPTOR_TYPE_RW_BUFFER, 2, 1, SHADER_STAGE_FRAG, "OutputBuffer"),
    };
    ShaderVariable fragmentVariables[] = {
        makeVariable("ViewProj", 0, 0, 64),
        makeVariable("Exposure", 0, 80, 4),
    };

    ShaderReflection stages[2] = {};
    stages[0].mShaderStage = SHADER_STAGE_VERT;
    stages[0].pShaderResources = vertexResources;
    stages[0].mShaderResourceCount = TF_ARRAY_COUNT(vertexResources);
    stages[0].pVariables = vertexVariables;
    stages[0].mVariableCount = TF_ARRAY_COUNT(vertexVariables);

    stages[1].mShaderStage = SHADER_STAGE_FRAG;
    stages[1].pShaderResources = fragmentResources;
    stages[1].mShaderResourceCount = TF_ARRAY_COUNT(fragmentResources);
    stages[1].pVariables = fragmentVariables;
    stages[1].mVariableCount = TF_ARRAY_COUNT(fragmentVariables);

    PipelineReflection pipeline = {};
    createPipelineReflection(stages, TF_ARRAY_COUNT(stages), &pipeline);

    ASSERT_EQ(pipeline.mStageReflectionCount, 2u);
    EXPECT_EQ(pipeline.mShaderStages, SHADER_STAGE_VERT | SHADER_STAGE_FRAG);
    EXPECT_EQ(pipeline.mVertexStageIndex, 0u);
    EXPECT_EQ(pipeline.mPixelStageIndex, 1u);
    EXPECT_EQ(pipeline.mHullStageIndex, UINT32_MAX);
    EXPECT_EQ(pipeline.mDomainStageIndex, UINT32_MAX);
    EXPECT_EQ(pipeline.mGeometryStageIndex, UINT32_MAX);

    ASSERT_EQ(pipeline.mShaderResourceCount, 3u);
    const uint32_t frameDataIndex = findResourceIndexByName(pipeline, "FrameData");
    const uint32_t sceneTextureIndex = findResourceIndexByName(pipeline, "SceneTexture");
    const uint32_t outputBufferIndex = findResourceIndexByName(pipeline, "OutputBuffer");
    ASSERT_NE(frameDataIndex, UINT32_MAX);
    ASSERT_NE(sceneTextureIndex, UINT32_MAX);
    ASSERT_NE(outputBufferIndex, UINT32_MAX);

    EXPECT_EQ(pipeline.pShaderResources[frameDataIndex].used_stages, SHADER_STAGE_VERT | SHADER_STAGE_FRAG);
    EXPECT_EQ(pipeline.pShaderResources[sceneTextureIndex].used_stages, SHADER_STAGE_VERT | SHADER_STAGE_FRAG);
    EXPECT_EQ(pipeline.pShaderResources[outputBufferIndex].used_stages, SHADER_STAGE_FRAG);

    ASSERT_EQ(pipeline.mVariableCount, 3u);
    const uint32_t viewProjIndex = findVariableIndexByName(pipeline, "ViewProj");
    const uint32_t cameraPosIndex = findVariableIndexByName(pipeline, "CameraPos");
    const uint32_t exposureIndex = findVariableIndexByName(pipeline, "Exposure");
    ASSERT_NE(viewProjIndex, UINT32_MAX);
    ASSERT_NE(cameraPosIndex, UINT32_MAX);
    ASSERT_NE(exposureIndex, UINT32_MAX);

    EXPECT_EQ(pipeline.pVariables[viewProjIndex].parent_index, frameDataIndex);
    EXPECT_EQ(pipeline.pVariables[cameraPosIndex].parent_index, frameDataIndex);
    EXPECT_EQ(pipeline.pVariables[exposureIndex].parent_index, frameDataIndex);

    tf_free(pipeline.pShaderResources);
    tf_free(pipeline.pVariables);
}

// Verifies that descriptor-name helpers detect root constants and root CBVs regardless of letter case.
TEST(RHIShaderReflectionTest, DescriptorNameHelpersRecognizeRootDescriptorsCaseInsensitively)
{
    EXPECT_TRUE(isDescriptorRootConstant("PerFrameRootConstant"));
    EXPECT_TRUE(isDescriptorRootConstant("material_pushconstant_data"));
    EXPECT_FALSE(isDescriptorRootConstant("SceneBuffer"));

    EXPECT_TRUE(isDescriptorRootCbv("CameraRootCbv"));
    EXPECT_TRUE(isDescriptorRootCbv("LIGHT_ROOTCBV_SLOT"));
    EXPECT_FALSE(isDescriptorRootCbv("RootConstantOnly"));
}
