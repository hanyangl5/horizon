#include "DeferredRendererPasses.h"

#include "Runtime/RHI/Private/RendererResourceAPI.h"

namespace
{
constexpr uint32_t kGeneratedVertexCount = 28;
constexpr uint32_t kGeneratedIndexCount = 42;

constexpr char kGeometryBuildShader[] = R"(
struct GeneratedVertex
{
    float4 Position;
    float4 Normal;
    float4 Color;
};

struct GeneratedVertexData
{
    float3 Position;
    float3 Normal;
    float3 Color;
};

RWStructuredBuffer<GeneratedVertex> GeneratedVertices : register(u0);
RWStructuredBuffer<uint> GeneratedIndices : register(u1);

GeneratedVertex PackVertex(GeneratedVertexData source)
{
    GeneratedVertex v;
    v.Position = float4(source.Position, 1.0f);
    v.Normal = float4(source.Normal, 0.0f);
    v.Color = float4(source.Color, 1.0f);
    return v;
}

GeneratedVertex MakeVertex(uint id)
{
    GeneratedVertexData v;
    v.Position = float3(0.0f, 0.0f, 0.0f);
    v.Normal = float3(0.0f, 1.0f, 0.0f);
    v.Color = float3(1.0f, 1.0f, 1.0f);

    switch (id)
    {
    case 0: v.Position = float3(-1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 1: v.Position = float3(1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 2: v.Position = float3(1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 3: v.Position = float3(-1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 0.0f, 1.0f); v.Color = float3(0.85f, 0.18f, 0.12f); break;
    case 4: v.Position = float3(1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 5: v.Position = float3(-1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 6: v.Position = float3(-1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 7: v.Position = float3(1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 0.0f, -1.0f); v.Color = float3(0.12f, 0.38f, 0.85f); break;
    case 8: v.Position = float3(-1.0f, -1.0f, -1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 9: v.Position = float3(-1.0f, -1.0f, 1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 10: v.Position = float3(-1.0f, 1.0f, 1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 11: v.Position = float3(-1.0f, 1.0f, -1.0f); v.Normal = float3(-1.0f, 0.0f, 0.0f); v.Color = float3(0.18f, 0.72f, 0.36f); break;
    case 12: v.Position = float3(1.0f, -1.0f, 1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 13: v.Position = float3(1.0f, -1.0f, -1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 14: v.Position = float3(1.0f, 1.0f, -1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 15: v.Position = float3(1.0f, 1.0f, 1.0f); v.Normal = float3(1.0f, 0.0f, 0.0f); v.Color = float3(0.92f, 0.66f, 0.20f); break;
    case 16: v.Position = float3(-1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 17: v.Position = float3(1.0f, 1.0f, 1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 18: v.Position = float3(1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 19: v.Position = float3(-1.0f, 1.0f, -1.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.75f, 0.28f, 0.88f); break;
    case 20: v.Position = float3(-1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 21: v.Position = float3(1.0f, -1.0f, -1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 22: v.Position = float3(1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 23: v.Position = float3(-1.0f, -1.0f, 1.0f); v.Normal = float3(0.0f, -1.0f, 0.0f); v.Color = float3(0.20f, 0.68f, 0.78f); break;
    case 24: v.Position = float3(-5.0f, 0.0f, -5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    case 25: v.Position = float3(5.0f, 0.0f, -5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    case 26: v.Position = float3(5.0f, 0.0f, 5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    case 27: v.Position = float3(-5.0f, 0.0f, 5.0f); v.Normal = float3(0.0f, 1.0f, 0.0f); v.Color = float3(0.42f, 0.48f, 0.50f); break;
    }

    return PackVertex(v);
}

uint MakeIndex(uint id)
{
    switch (id)
    {
    case 0: return 0; case 1: return 1; case 2: return 2; case 3: return 0; case 4: return 2; case 5: return 3;
    case 6: return 4; case 7: return 5; case 8: return 6; case 9: return 4; case 10: return 6; case 11: return 7;
    case 12: return 8; case 13: return 9; case 14: return 10; case 15: return 8; case 16: return 10; case 17: return 11;
    case 18: return 12; case 19: return 13; case 20: return 14; case 21: return 12; case 22: return 14; case 23: return 15;
    case 24: return 16; case 25: return 17; case 26: return 18; case 27: return 16; case 28: return 18; case 29: return 19;
    case 30: return 20; case 31: return 21; case 32: return 22; case 33: return 20; case 34: return 22; case 35: return 23;
    case 36: return 0; case 37: return 1; case 38: return 2; case 39: return 0; case 40: return 2; case 41: return 3;
    default: return 0;
    }
}

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint id = dispatchThreadId.x;
    if (id < 28)
        GeneratedVertices[id] = MakeVertex(id);
    if (id < 42)
        GeneratedIndices[id] = MakeIndex(id);
}
)";
} // namespace

bool GeometryBuildPass::init(Renderer* pRenderer)
{
    ShaderSrcDesc shaderDesc = {
        .mStages = SHADER_STAGE_COMP,
        .mComp = {
            .pName = "DeferredRendererGeometryBuildCS",
            .pByteCode = const_cast<char*>(kGeometryBuildShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kGeometryBuildShader) - 1),
            .pEntryPoint = "CSMain",
        },
    };
    addShaderSource(pRenderer, &shaderDesc, &pShader);
    if (!pShader)
        return false;

    Shader* shaders[] = { pShader };
    RootSignatureDesc rootDesc = {
        .ppShaders = shaders,
        .mShaderCount = 1,
    };
    addRootSignature(pRenderer, &rootDesc, &pRootSignature);
    return pRootSignature != nullptr;
}

void GeometryBuildPass::exit(Renderer* pRenderer)
{
    if (pRootSignature)
    {
        removeRootSignature(pRenderer, pRootSignature);
        pRootSignature = nullptr;
    }
    if (pShader)
    {
        removeShader(pRenderer, pShader);
        pShader = nullptr;
    }
}

bool GeometryBuildPass::createDescriptorSet(Renderer* pRenderer, uint32_t frameResourceCount)
{
    DescriptorSetDesc setDesc = {
        .pRootSignature = pRootSignature,
        .mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE,
        .mMaxSets = frameResourceCount,
        .mNodeIndex = pRenderer->mUnlinkedRendererIndex,
    };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
    return pDescriptorSet != nullptr;
}

void GeometryBuildPass::removeDescriptorSet(Renderer* pRenderer)
{
    if (pDescriptorSet)
    {
        ::removeDescriptorSet(pRenderer, pDescriptorSet);
        pDescriptorSet = nullptr;
    }
}

bool GeometryBuildPass::addPipeline(Renderer* pRenderer)
{
    PipelineDesc pipelineDesc = {
        .mComputeDesc = {
            .pShaderProgram = pShader,
            .pRootSignature = pRootSignature,
        },
        .pName = "DeferredRenderer.GeometryBuildPipeline",
        .mType = PIPELINE_TYPE_COMPUTE,
    };
    ::addPipeline(pRenderer, &pipelineDesc, &pPipeline);
    return pPipeline != nullptr;
}

void GeometryBuildPass::removePipeline(Renderer* pRenderer)
{
    if (pPipeline)
    {
        ::removePipeline(pRenderer, pPipeline);
        pPipeline = nullptr;
    }
}

bool GeometryBuildPass::isReady() const
{
    return pPipeline && pDescriptorSet;
}

void GeometryBuildPass::createFrameResources(RenderGraph& graph, RGFrameData& frameData, Renderer* pRenderer) const
{
    BufferDesc vertexDesc = {
        .mSize = sizeof(Vertex) * kGeneratedVertexCount,
        .mFirstElement = 0,
        .mElementCount = kGeneratedVertexCount,
        .mStructStride = sizeof(Vertex),
        .pName = "DeferredRenderer.GeneratedVertices",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .mFlags = BUFFER_CREATION_FLAG_NONE,
        .mStartState = RESOURCE_STATE_UNORDERED_ACCESS,
        .mDescriptors = static_cast<DescriptorType>(DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER),
        .mNodeIndex = pRenderer->mUnlinkedRendererIndex,
    };
    BufferDesc indexDesc = {
        .mSize = sizeof(uint32_t) * kGeneratedIndexCount,
        .mFirstElement = 0,
        .mElementCount = kGeneratedIndexCount,
        .mStructStride = sizeof(uint32_t),
        .pName = "DeferredRenderer.GeneratedIndices",
        .mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
        .mFlags = BUFFER_CREATION_FLAG_NONE,
        .mStartState = RESOURCE_STATE_UNORDERED_ACCESS,
        .mDescriptors = static_cast<DescriptorType>(DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER),
        .mNodeIndex = pRenderer->mUnlinkedRendererIndex,
    };
    frameData.generatedVertices = graph.createBuffer("GeneratedVertices", &vertexDesc);
    frameData.generatedIndices = graph.createBuffer("GeneratedIndices", &indexDesc);
}

void GeometryBuildPass::record(RenderGraph& graph, const RGFrameData& frameData, uint32_t frameResourceIndex,
                               ProfileToken gpuProfileToken)
{
    mRecordContext = { frameData, frameResourceIndex, gpuProfileToken };

    graph.addComputePass("BuildGeometry")
        .write(frameData.generatedVertices, RESOURCE_STATE_UNORDERED_ACCESS)
        .write(frameData.generatedIndices, RESOURCE_STATE_UNORDERED_ACCESS)
        .setExecute(RDG_EXECUTE(GeometryBuildPass, this,
        {
            Buffer* generatedVertices = context.getBuffer(self->mRecordContext.frameData.generatedVertices);
            Buffer* generatedIndices = context.getBuffer(self->mRecordContext.frameData.generatedIndices);
            if (!generatedVertices || !generatedIndices)
                return;

            DescriptorData params[2] = {};
            params[0].pName = "GeneratedVertices";
            params[0].ppBuffers = &generatedVertices;
            params[1].pName = "GeneratedIndices";
            params[1].ppBuffers = &generatedIndices;
            updateDescriptorSet(context.getRenderer(), self->mRecordContext.frameResourceIndex, self->pDescriptorSet, 2, params);

            cmdBeginGpuTimestampQuery(pPassCmd, self->mRecordContext.gpuProfileToken, "RDG Build Geometry");
            cmdBindPipeline(pPassCmd, self->pPipeline);
            cmdBindDescriptorSet(pPassCmd, self->mRecordContext.frameResourceIndex, self->pDescriptorSet);
            cmdDispatch(pPassCmd, 1, 1, 1);
            cmdEndGpuTimestampQuery(pPassCmd, self->mRecordContext.gpuProfileToken);
        }));
}
