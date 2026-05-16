#include "DeferredRendererPasses.h"

#include "Runtime/RHI/Private/RendererResourceAPI.h"

namespace
{
constexpr uint32_t kCubeIndexCount = 36;
constexpr uint32_t kPlaneIndexCount = 6;
constexpr uint32_t kPlaneFirstIndex = 36;
constexpr uint32_t kPlaneFirstVertex = 24;

constexpr char kGBufferShader[] = R"(
#pragma pack_matrix(column_major)

cbuffer SceneUniforms : register(b0)
{
    float4x4 WorldViewProjection;
    float4x4 World;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float3 Color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(WorldViewProjection, float4(input.Position, 1.0f));
    output.Normal = normalize(mul(World, float4(input.Normal, 0.0f)).xyz);
    output.Color = input.Color;
    return output;
}

struct GBufferOutput
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

GBufferOutput PSMain(VSOutput input)
{
    GBufferOutput output;
    output.Albedo = float4(input.Color, 1.0f);
    output.Normal = float4(normalize(input.Normal) * 0.5f + 0.5f, 1.0f);
    return output;
}
)";
} // namespace

bool GBufferPass::init(Renderer* pRenderer)
{
    ShaderSrcDesc shaderDesc = {
        .mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        .mVert = {
            .pName = "DeferredRendererGBufferVS",
            .pByteCode = const_cast<char*>(kGBufferShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kGBufferShader) - 1),
            .pEntryPoint = "VSMain",
        },
        .mFrag = {
            .pName = "DeferredRendererGBufferPS",
            .pByteCode = const_cast<char*>(kGBufferShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kGBufferShader) - 1),
            .pEntryPoint = "PSMain",
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

void GBufferPass::exit(Renderer* pRenderer)
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

bool GBufferPass::createDescriptorSet(Renderer* pRenderer, Buffer* pSceneUniformBuffers[kFrameResourceCount][kSceneObjectCount],
                                      uint32_t frameResourceCount)
{
    DescriptorSetDesc setDesc = {
        .pRootSignature = pRootSignature,
        .mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE,
        .mMaxSets = frameResourceCount * kSceneObjectCount,
    };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
    if (!pDescriptorSet)
        return false;

    for (uint32_t frame = 0; frame < frameResourceCount; ++frame)
    {
        for (uint32_t object = 0; object < kSceneObjectCount; ++object)
        {
            DescriptorData params[1] = {};
            params[0].pName = "SceneUniforms";
            params[0].ppBuffers = &pSceneUniformBuffers[frame][object];
            updateDescriptorSet(pRenderer, frame * kSceneObjectCount + object, pDescriptorSet, 1, params);
        }
    }

    return true;
}

void GBufferPass::removeDescriptorSet(Renderer* pRenderer)
{
    if (pDescriptorSet)
    {
        ::removeDescriptorSet(pRenderer, pDescriptorSet);
        pDescriptorSet = nullptr;
    }
}

bool GBufferPass::addPipeline(Renderer* pRenderer)
{
    VertexLayout vertexLayout = {
        .mBindings = {
            {
                .mStride = sizeof(Vertex),
                .mRate = VERTEX_BINDING_RATE_VERTEX,
            },
        },
        .mAttribs = {
            {
                .mSemantic = SEMANTIC_POSITION,
                .mFormat = TinyImageFormat_R32G32B32_SFLOAT,
                .mBinding = 0,
                .mLocation = 0,
                .mOffset = offsetof(Vertex, position),
            },
            {
                .mSemantic = SEMANTIC_NORMAL,
                .mFormat = TinyImageFormat_R32G32B32_SFLOAT,
                .mBinding = 0,
                .mLocation = 1,
                .mOffset = offsetof(Vertex, normal),
            },
            {
                .mSemantic = SEMANTIC_COLOR,
                .mFormat = TinyImageFormat_R32G32B32_SFLOAT,
                .mBinding = 0,
                .mLocation = 2,
                .mOffset = offsetof(Vertex, color),
            },
        },
        .mBindingCount = 1,
        .mAttribCount = 3,
    };

    RasterizerStateDesc rasterizerDesc = {
        .mCullMode = CULL_MODE_NONE,
    };
    DepthStateDesc depthDesc = {
        .mDepthTest = true,
        .mDepthWrite = true,
        .mDepthFunc = CMP_LEQUAL,
    };
    TinyImageFormat gbufferFormats[] = { TinyImageFormat_R8G8B8A8_UNORM, TinyImageFormat_R16G16B16A16_SFLOAT };
    PipelineDesc pipelineDesc = {
        .mGraphicsDesc = {
            .pShaderProgram = pShader,
            .pRootSignature = pRootSignature,
            .pVertexLayout = &vertexLayout,
            .pDepthState = &depthDesc,
            .pRasterizerState = &rasterizerDesc,
            .pColorFormats = gbufferFormats,
            .mRenderTargetCount = 2,
            .mSampleCount = SAMPLE_COUNT_1,
            .mSampleQuality = 0,
            .mDepthStencilFormat = TinyImageFormat_D32_SFLOAT,
            .mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST,
        },
        .pName = "DeferredRenderer.GBufferPipeline",
        .mType = PIPELINE_TYPE_GRAPHICS,
    };
    ::addPipeline(pRenderer, &pipelineDesc, &pPipeline);
    return pPipeline != nullptr;
}

void GBufferPass::removePipeline(Renderer* pRenderer)
{
    if (pPipeline)
    {
        ::removePipeline(pRenderer, pPipeline);
        pPipeline = nullptr;
    }
}

bool GBufferPass::isReady() const
{
    return pPipeline && pDescriptorSet;
}

void GBufferPass::createFrameResources(RenderGraph& graph, RGFrameData& frameData, Renderer* pRenderer, uint32_t width,
                                       uint32_t height) const
{
    RenderTargetDesc albedoDesc = {
        .mWidth = width,
        .mHeight = height,
        .mDepth = 1,
        .mArraySize = 1,
        .mMipLevels = 1,
        .mSampleCount = SAMPLE_COUNT_1,
        .mFormat = TinyImageFormat_R8G8B8A8_UNORM,
        .mStartState = RESOURCE_STATE_RENDER_TARGET,
        .mClearValue = {},
        .mSampleQuality = 0,
        .mDescriptors = DESCRIPTOR_TYPE_TEXTURE,
        .pName = "GBuffer.Albedo",
    };
    RenderTargetDesc normalDesc = {
        .mWidth = width,
        .mHeight = height,
        .mDepth = 1,
        .mArraySize = 1,
        .mMipLevels = 1,
        .mSampleCount = SAMPLE_COUNT_1,
        .mFormat = TinyImageFormat_R16G16B16A16_SFLOAT,
        .mStartState = RESOURCE_STATE_RENDER_TARGET,
        .mClearValue = {},
        .mSampleQuality = 0,
        .mDescriptors = DESCRIPTOR_TYPE_TEXTURE,
        .pName = "GBuffer.Normal",
    };
    RenderTargetDesc depthDesc = {
        .mWidth = width,
        .mHeight = height,
        .mDepth = 1,
        .mArraySize = 1,
        .mMipLevels = 1,
        .mSampleCount = SAMPLE_COUNT_1,
        .mFormat = TinyImageFormat_D32_SFLOAT,
        .mStartState = RESOURCE_STATE_DEPTH_WRITE,
        .mClearValue = {},
        .mSampleQuality = 0,
        .mDescriptors = DESCRIPTOR_TYPE_TEXTURE,
        .pName = "GBuffer.Depth",
    };

    frameData.gbufferAlbedo = graph.createRenderTarget("GBuffer.Albedo", &albedoDesc);
    frameData.gbufferNormal = graph.createRenderTarget("GBuffer.Normal", &normalDesc);
    frameData.gbufferDepth = graph.createRenderTarget("GBuffer.Depth", &depthDesc);
}

void GBufferPass::record(RenderGraph& graph, const RGFrameData& frameData, Buffer* const* ppSceneUniformBuffers,
                         ProfileToken gpuProfileToken)
{
    mRecordContext = { frameData, gpuProfileToken };

    RGBuffer sceneUniforms[kSceneObjectCount] = {};
    for (uint32_t i = 0; i < kSceneObjectCount; ++i)
    {
        const char* pName = i == 0 ? "SceneUniforms.Cube" : "SceneUniforms.Floor";
        sceneUniforms[i] = graph.importBuffer(pName, ppSceneUniformBuffers[i], RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_GENERIC_READ);
    }

    ClearValue blackClear = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f };
    ClearValue normalClear = { .r = 0.5f, .g = 0.5f, .b = 1.0f, .a = 1.0f };
    ClearValue depthClear = { .depth = 1.0f, .stencil = 0 };

    graph.addRasterPass("GBuffer")
        .read(frameData.generatedVertices, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        .read(frameData.generatedIndices, RESOURCE_STATE_INDEX_BUFFER)
        .read(sceneUniforms[0], RESOURCE_STATE_GENERIC_READ)
        .read(sceneUniforms[1], RESOURCE_STATE_GENERIC_READ)
        .writeRenderTarget(frameData.gbufferAlbedo, 0u, LOAD_ACTION_CLEAR, STORE_ACTION_STORE, blackClear, true)
        .writeRenderTarget(frameData.gbufferNormal, 1u, LOAD_ACTION_CLEAR, STORE_ACTION_STORE, normalClear, true)
        .writeDepthStencil(frameData.gbufferDepth, LOAD_ACTION_CLEAR, STORE_ACTION_STORE, depthClear, true)
        .setExecute([this](Cmd* pPassCmd, const RGPassContext& context) {
            Buffer* vertexBuffer = context.getBuffer(mRecordContext.frameData.generatedVertices);
            Buffer* indexBuffer = context.getBuffer(mRecordContext.frameData.generatedIndices);
            if (!vertexBuffer || !indexBuffer)
                return;

            cmdBeginGpuTimestampQuery(pPassCmd, mRecordContext.gpuProfileToken, "RDG GBuffer");
            cmdSetViewport(pPassCmd, 0.0f, 0.0f, static_cast<float>(context.getWidth()), static_cast<float>(context.getHeight()),
                           0.0f, 1.0f);
            cmdSetScissor(pPassCmd, 0, 0, context.getWidth(), context.getHeight());
            cmdBindPipeline(pPassCmd, pPipeline);
            Buffer* vertexBuffers[] = { vertexBuffer };
            uint32_t vertexStrides[] = { static_cast<uint32_t>(sizeof(Vertex)) };
            uint64_t vertexOffsets[] = { 0 };
            cmdBindVertexBuffer(pPassCmd, 1, vertexBuffers, vertexStrides, vertexOffsets);
            cmdBindIndexBuffer(pPassCmd, indexBuffer, INDEX_TYPE_UINT32, 0);
            const uint32_t objectSetBase = context.getFrameResourceIndex() * kSceneObjectCount;
            cmdBindDescriptorSet(pPassCmd, objectSetBase, pDescriptorSet);
            cmdDrawIndexed(pPassCmd, kCubeIndexCount, 0, 0);
            cmdBindDescriptorSet(pPassCmd, objectSetBase + 1, pDescriptorSet);
            cmdDrawIndexed(pPassCmd, kPlaneIndexCount, kPlaneFirstIndex, kPlaneFirstVertex);
            cmdEndGpuTimestampQuery(pPassCmd, mRecordContext.gpuProfileToken);
        });
}
