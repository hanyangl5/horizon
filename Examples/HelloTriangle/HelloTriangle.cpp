#include <cstddef>
#include <cstdint>
#include <optional>

#include "Application/IApp.h"
#include "Core/ILog.h"
#include "Graphics/RenderContext.h"
#include "Profiler/IProfiler.h"

namespace
{
constexpr uint32_t        kCpuProfileColor = 0x3399FF;
constexpr TinyImageFormat kSurfaceFormat = TinyImageFormat_B8G8R8A8_SRGB;

struct Vertex
{
    float position[2];
    float color[3];
};

constexpr Vertex kTriangleVertices[] = {
    { { 0.0f, 0.65f }, { 1.0f, 0.2f, 0.2f } },
    { { 0.65f, -0.55f }, { 0.2f, 1.0f, 0.2f } },
    { { -0.65f, -0.55f }, { 0.2f, 0.4f, 1.0f } },
};

constexpr char kHelloTriangleShader[] = R"(
struct VSInput
{
    float2 Position : POSITION;
    float3 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float3 Color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = float4(input.Position, 0.0f, 1.0f);
    output.Color = input.Color;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return float4(input.Color, 1.0f);
}
)";

} // namespace

class HelloTriangleApp final: public IApp
{
public:
    HelloTriangleApp()
    {
        mSettings.mWidth = 1280;
        mSettings.mHeight = 720;
        mSettings.mVSyncEnabled = true;
        mSettings.mShowPlatformUI = false;
    }

    bool Init() override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Init", kCpuProfileColor);

        hz::ContextDesc contextDesc = {
            .pAppName = GetName(),
            .windowHandle = pWindow->handle,
            .width = 0,
            .height = 0,
            .imageCount = 3,
            .colorFormat = kSurfaceFormat,
            .colorSpace = COLOR_SPACE_SDR_SRGB,
            .enableVSync = mSettings.mVSyncEnabled,
            .enableGpuValidation = true,
            .enableGpuProfiler = true,
        };
        const bool initialized = context.init(contextDesc);
        ASSERT(initialized);
        if (!initialized)
            return false;
        vSync = mSettings.mVSyncEnabled;

        resources.emplace();
        return createResources();
    }

    void Exit() override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Exit", kCpuProfileColor);
        context.waitIdle();
        resources.reset();
    }

    bool Load(ReloadDesc* pReloadDesc) override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Load", kCpuProfileColor);
        if (!(pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET)))
            return true;

        const uint32_t width = (uint32_t)mSettings.mWidth;
        const uint32_t height = (uint32_t)mSettings.mHeight;
        if (!context.resize(width, height))
            return false;
        return true;
    }

    void Unload(ReloadDesc*) override {}

    void Update(float) override { PROFILER_SET_CPU_SCOPE("HelloTriangle", "Update", kCpuProfileColor); }

    void Draw() override
    {
        PROFILER_SET_CPU_SCOPE("HelloTriangle", "Draw", kCpuProfileColor);
        if (vSync != mSettings.mVSyncEnabled && context.setVSync(mSettings.mVSyncEnabled))
            vSync = mSettings.mVSyncEnabled;

        if (context.isSuspended())
            return;
        hz::CommandList&      commands = context.acquireCommandList();
        const hz::GPUTexture& backbuffer = context.getCurrentBackbuffer();

        hz::RenderPassDesc pass = {
            .colorAttachments = { {
                .pTexture = &backbuffer,
                .loadAction = LOAD_ACTION_CLEAR,
                .storeAction = STORE_ACTION_STORE,
                .clearValue = { .r = 0.05f, .g = 0.06f, .b = 0.08f, .a = 1.0f },
            } },
            .colorAttachmentCount = 1,
        };
        commands.beginRendering(pass);
        commands.beginGpuTimestamp("Triangle Pass");
        commands.setViewport(0.0f, 0.0f, (float)context.getWidth(), (float)context.getHeight());
        commands.setScissor(0, 0, context.getWidth(), context.getHeight());
        commands.setPipeline(resources->pipeline);
        commands.setVertexBuffer(0, resources->vertexBuffer, 0, sizeof(Vertex));
        commands.draw(3);
        commands.endGpuTimestamp();
        commands.endRendering();
        context.submit(commands, &backbuffer);
    }

    const char* GetName() override { return "HelloTriangle"; }

private:
    bool createResources()
    {
        hz::BufferDesc bufferDesc = {
            .size = sizeof(kTriangleVertices),
            .elementCount = sizeof(kTriangleVertices) / sizeof(uint32_t),
            .pName = "HelloTriangle.VertexBuffer",
            .pInitialData = kTriangleVertices,
            .initialDataSize = sizeof(kTriangleVertices),
            .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
            .startState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            .descriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_VERTEX_BUFFER,
            .flags = BUFFER_CREATION_FLAG_NONE,
        };
        resources->vertexBuffer = context.createBuffer(bufferDesc);
        ASSERT(resources->vertexBuffer);

        hz::ShaderDesc shaderDesc = {
            .stages = {
                {
                    .stage = SHADER_STAGE_VERT,
                    .pSource = kHelloTriangleShader,
                    .sourceSize = (uint32_t)(sizeof(kHelloTriangleShader) - 1),
                    .pEntryPoint = "VSMain",
                    .pName = "HelloTriangleVS",
                },
                {
                    .stage = SHADER_STAGE_FRAG,
                    .pSource = kHelloTriangleShader,
                    .sourceSize = (uint32_t)(sizeof(kHelloTriangleShader) - 1),
                    .pEntryPoint = "PSMain",
                    .pName = "HelloTrianglePS",
                },
            },
            .stageCount = 2,
        };
        resources->shader = context.createShader(shaderDesc);
        ASSERT(resources->shader);
        hz::GraphicsPipelineDesc pipelineDesc = {
            .pShader = &resources->shader,
            .vertexLayout = {
                .mBindings = { { .mStride = sizeof(Vertex), .mRate = VERTEX_BINDING_RATE_VERTEX } },
                .mAttribs = {
                    { .mSemantic = SEMANTIC_POSITION, .mFormat = TinyImageFormat_R32G32_SFLOAT, .mBinding = 0, .mLocation = 0,
                      .mOffset = (uint32_t)offsetof(Vertex, position) },
                    { .mSemantic = SEMANTIC_COLOR, .mFormat = TinyImageFormat_R32G32B32_SFLOAT, .mBinding = 0, .mLocation = 1,
                      .mOffset = (uint32_t)offsetof(Vertex, color) },
                },
                .mBindingCount = 1,
                .mAttribCount = 2,
            },
            .rasterizer = { .mCullMode = CULL_MODE_NONE, .mFillMode = FILL_MODE_SOLID },
            .depth = { .mDepthFunc = CMP_ALWAYS },
            .blend = {
                .mSrcFactors = { BC_ONE },
                .mDstFactors = { BC_ZERO },
                .mSrcAlphaFactors = { BC_ONE },
                .mDstAlphaFactors = { BC_ZERO },
                .mBlendModes = { BM_ADD },
                .mBlendAlphaModes = { BM_ADD },
                .mColorWriteMasks = { COLOR_MASK_ALL },
                .mRenderTargetMask = BLEND_STATE_TARGET_0,
            },
            .colorFormats = { kSurfaceFormat },
            .renderTargetCount = 1,
            .topology = PRIMITIVE_TOPO_TRI_LIST,
            .sampleCount = SAMPLE_COUNT_1,
            .pName = "HelloTriangle.Pipeline",
        };
        resources->pipeline = context.createGraphicsPipeline(pipelineDesc);
        ASSERT(resources->pipeline);
        return true;
    }

    struct Resources
    {
        hz::GPUBuffer   vertexBuffer;
        hz::GPUShader   shader;
        hz::GPUPipeline pipeline;
    };

    hz::RenderContext        context;
    std::optional<Resources> resources;
    bool                     vSync = true;
};

DEFINE_APPLICATION_MAIN(HelloTriangleApp)
