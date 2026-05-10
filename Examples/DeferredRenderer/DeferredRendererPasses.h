#pragma once

#include <cstddef>
#include <cstdint>

#include "Core/IMath.h"
#include "Graphics/IRenderGraph.h"
#include "Profiler/IProfiler.h"
#include "RHI/IGraphics.h"

constexpr uint32_t kFrameResourceCount = 2;
constexpr uint32_t kSceneObjectCount = 2;
constexpr uint32_t kConstantBufferSize = 256;

struct Vertex
{
    Vector3 position;
    Vector3 normal;
    Vector3 color;
};

static_assert(sizeof(Vertex) == 48);
static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, normal) == 16);
static_assert(offsetof(Vertex, color) == 32);

struct SceneUniforms
{
    Matrix4 worldViewProjection;
    Matrix4 world;
};

struct RGFrameData
{
    RGTexture backbuffer;
    RGTexture gbufferAlbedo;
    RGTexture gbufferNormal;
    RGTexture gbufferDepth;
    RGBuffer  generatedVertices;
    RGBuffer  generatedIndices;
};

class GeometryBuildPass
{
public:
    bool init(Renderer* pRenderer);
    void exit(Renderer* pRenderer);

    bool createDescriptorSet(Renderer* pRenderer, uint32_t frameResourceCount);
    void removeDescriptorSet(Renderer* pRenderer);

    bool addPipeline(Renderer* pRenderer);
    void removePipeline(Renderer* pRenderer);

    bool isReady() const;
    void createFrameResources(RenderGraph& graph, RGFrameData& frameData, Renderer* pRenderer) const;
    void record(RenderGraph& graph, const RGFrameData& frameData, uint32_t frameResourceIndex, ProfileToken gpuProfileToken);

private:
    struct RecordContext
    {
        RGFrameData  frameData = {};
        uint32_t     frameResourceIndex = 0;
        ProfileToken gpuProfileToken = PROFILE_INVALID_TOKEN;
    };

    Shader*        pShader = nullptr;
    RootSignature* pRootSignature = nullptr;
    DescriptorSet* pDescriptorSet = nullptr;
    Pipeline*      pPipeline = nullptr;
    RecordContext  mRecordContext = {};
};

class GBufferPass
{
public:
    bool init(Renderer* pRenderer);
    void exit(Renderer* pRenderer);

    bool createDescriptorSet(Renderer* pRenderer, Buffer* pSceneUniformBuffers[kFrameResourceCount][kSceneObjectCount],
                             uint32_t frameResourceCount);
    void removeDescriptorSet(Renderer* pRenderer);

    bool addPipeline(Renderer* pRenderer);
    void removePipeline(Renderer* pRenderer);

    bool isReady() const;
    void createFrameResources(RenderGraph& graph, RGFrameData& frameData, Renderer* pRenderer, uint32_t width, uint32_t height) const;
    void record(RenderGraph& graph, const RGFrameData& frameData, Buffer* const* ppSceneUniformBuffers, uint32_t frameResourceIndex,
                ProfileToken gpuProfileToken);

private:
    struct RecordContext
    {
        RGFrameData  frameData = {};
        uint32_t     frameResourceIndex = 0;
        ProfileToken gpuProfileToken = PROFILE_INVALID_TOKEN;
    };

    Shader*        pShader = nullptr;
    RootSignature* pRootSignature = nullptr;
    DescriptorSet* pDescriptorSet = nullptr;
    Pipeline*      pPipeline = nullptr;
    RecordContext  mRecordContext = {};
};

class LightingPass
{
public:
    bool init(Renderer* pRenderer);
    void exit(Renderer* pRenderer);

    bool createDescriptorSet(Renderer* pRenderer, uint32_t frameResourceCount);
    void removeDescriptorSet(Renderer* pRenderer);

    bool addPipeline(Renderer* pRenderer, TinyImageFormat swapchainFormat);
    void removePipeline(Renderer* pRenderer);

    bool isReady() const;
    void record(RenderGraph& graph, const RGFrameData& frameData, uint32_t frameResourceIndex, ProfileToken gpuProfileToken);

private:
    struct RecordContext
    {
        RGFrameData  frameData = {};
        uint32_t     frameResourceIndex = 0;
        ProfileToken gpuProfileToken = PROFILE_INVALID_TOKEN;
    };

    Shader*        pShader = nullptr;
    RootSignature* pRootSignature = nullptr;
    DescriptorSet* pDescriptorSet = nullptr;
    Pipeline*      pPipeline = nullptr;
    RecordContext  mRecordContext = {};
};
