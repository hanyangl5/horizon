#include "DeferredRendererPasses.h"

#include "Runtime/RHI/Private/RendererResourceAPI.h"

namespace
{
constexpr char kLightingShader[] = R"(
Texture2D<float4> AlbedoTexture : register(t0);
Texture2D<float4> NormalTexture : register(t1);
Texture2D<float> DepthTexture : register(t2);

struct VSOutput
{
    float4 Position : SV_Position;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f),
    };

    VSOutput output;
    output.Position = float4(positions[vertexId], 0.0f, 1.0f);
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    int2 pixel = int2(input.Position.xy);
    float4 albedo = AlbedoTexture.Load(int3(pixel, 0));
    float3 normal = normalize(NormalTexture.Load(int3(pixel, 0)).xyz * 2.0f - 1.0f);
    float depth = DepthTexture.Load(int3(pixel, 0));

    float3 lightDir = normalize(float3(0.45f, 0.85f, -0.25f));
    float ndotl = saturate(dot(normal, lightDir));
    float3 color = albedo.rgb * (0.16f + ndotl * 0.84f);
    color += saturate(1.0f - depth) * 0.04f;
    float geometryMask = saturate(dot(albedo.rgb, float3(32.0f, 32.0f, 32.0f)));
    color = lerp(float3(0.02f, 0.025f, 0.03f), color, geometryMask);
    return float4(color, 1.0f);
}
)";
} // namespace

bool LightingPass::init(Renderer* pRenderer)
{
    ShaderSrcDesc shaderDesc = {
        .mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        .mVert = {
            .pName = "DeferredRendererLightingVS",
            .pByteCode = const_cast<char*>(kLightingShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kLightingShader) - 1),
            .pEntryPoint = "VSMain",
        },
        .mFrag = {
            .pName = "DeferredRendererLightingPS",
            .pByteCode = const_cast<char*>(kLightingShader),
            .mByteCodeSize = static_cast<uint32_t>(sizeof(kLightingShader) - 1),
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

void LightingPass::exit(Renderer* pRenderer)
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

bool LightingPass::createDescriptorSet(Renderer* pRenderer, uint32_t frameResourceCount)
{
    DescriptorSetDesc setDesc = {
        .pRootSignature = pRootSignature,
        .mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE,
        .mMaxSets = frameResourceCount,
    };
    addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
    return pDescriptorSet != nullptr;
}

void LightingPass::removeDescriptorSet(Renderer* pRenderer)
{
    if (pDescriptorSet)
    {
        ::removeDescriptorSet(pRenderer, pDescriptorSet);
        pDescriptorSet = nullptr;
    }
}

bool LightingPass::addPipeline(Renderer* pRenderer, TinyImageFormat swapchainFormat)
{
    RasterizerStateDesc rasterizerDesc = {
        .mCullMode = CULL_MODE_NONE,
    };
    PipelineDesc pipelineDesc = {
        .mGraphicsDesc = {
            .pShaderProgram = pShader,
            .pRootSignature = pRootSignature,
            .pRasterizerState = &rasterizerDesc,
            .pColorFormats = &swapchainFormat,
            .mRenderTargetCount = 1,
            .mSampleCount = SAMPLE_COUNT_1,
            .mSampleQuality = 0,
            .mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST,
        },
        .pName = "DeferredRenderer.LightingPipeline",
        .mType = PIPELINE_TYPE_GRAPHICS,
    };
    ::addPipeline(pRenderer, &pipelineDesc, &pPipeline);
    return pPipeline != nullptr;
}

void LightingPass::removePipeline(Renderer* pRenderer)
{
    if (pPipeline)
    {
        ::removePipeline(pRenderer, pPipeline);
        pPipeline = nullptr;
    }
}

bool LightingPass::isReady() const
{
    return pPipeline && pDescriptorSet;
}

void LightingPass::record(RenderGraph& graph, const RGFrameData& frameData, ProfileToken gpuProfileToken)
{
    mRecordContext = { frameData, gpuProfileToken };

    ClearValue backbufferClear = { .r = 0.02f, .g = 0.025f, .b = 0.03f, .a = 1.0f };

    graph.addRasterPass("Lighting")
        .read(frameData.gbufferAlbedo, RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        .read(frameData.gbufferNormal, RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        .read(frameData.gbufferDepth, RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        .writeRenderTarget(frameData.backbuffer, 0u, LOAD_ACTION_CLEAR, STORE_ACTION_STORE, backbufferClear, true)
        .setExecute([this](Cmd* pPassCmd, const RGPassContext& context) {
            Texture* textures[] = {
                context.getTexture(mRecordContext.frameData.gbufferAlbedo),
                context.getTexture(mRecordContext.frameData.gbufferNormal),
                context.getTexture(mRecordContext.frameData.gbufferDepth),
            };
            if (!textures[0] || !textures[1] || !textures[2])
                return;

            DescriptorData params[3] = {};
            params[0].pName = "AlbedoTexture";
            params[0].ppTextures = &textures[0];
            params[1].pName = "NormalTexture";
            params[1].ppTextures = &textures[1];
            params[2].pName = "DepthTexture";
            params[2].ppTextures = &textures[2];
            const uint32_t frameResourceIndex = context.getFrameResourceIndex();
            updateDescriptorSet(context.getRenderer(), frameResourceIndex, pDescriptorSet, 3, params);

            cmdBeginGpuTimestampQuery(pPassCmd, mRecordContext.gpuProfileToken, "RDG Lighting");
            cmdSetViewport(pPassCmd, 0.0f, 0.0f, static_cast<float>(context.getWidth()), static_cast<float>(context.getHeight()),
                           0.0f, 1.0f);
            cmdSetScissor(pPassCmd, 0, 0, context.getWidth(), context.getHeight());
            cmdBindPipeline(pPassCmd, pPipeline);
            cmdBindDescriptorSet(pPassCmd, frameResourceIndex, pDescriptorSet);
            cmdDraw(pPassCmd, 3, 0);
            cmdEndGpuTimestampQuery(pPassCmd, mRecordContext.gpuProfileToken);
        });
}
