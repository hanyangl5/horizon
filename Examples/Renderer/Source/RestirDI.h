#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Graphics/RenderContext.h"
#include "LightingTypes.h"
#include "Scene/SceneView.h"

namespace hz
{
class GpuDrawList;
}

// One local light as ReSTIR DI samples it, rebuilt from RenderScene every frame and read through the bindless heap.
// Mirrors RestirDILight in RestirDI.slang field for field.
struct alignas(16) RestirDILight
{
    Vector4  position;   // xyz emitter centre in world space, w range in metres (0 = unbounded).
    Vector4  direction;  // xyz unit direction the emitter faces (local -Z), w cos of the outer cone half angle.
    Vector4  color;      // rgb intensity in candela, w cos of the inner cone half angle.
    uint32_t type = 0;   // hz::LightType: Point or Spot.
    float    width = 0;  // Reserved for area lights.
    float    height = 0; // Reserved for area lights.
    float    area = 0;   // Reserved for area lights.
};
static_assert(sizeof(RestirDILight) == 64);

// One pixel's reservoir. The table holds delta lights, so the light index identifies the sample. Mirrors
// RestirDIReservoir in RestirDI.slang.
struct RestirDIReservoir
{
    uint32_t light;     // Light index in the low 16 bits, receiver material ID in the high 16 bits.
    float    weight;    // W = wSum / (M * pHat(selected)).
    float    count;     // M.
    float    viewDepth; // Receiver view depth, 0 for an empty reservoir.
};
static_assert(sizeof(RestirDIReservoir) == 16);

struct RestirDISettings
{
    // Candidates drawn uniformly from the light table per pixel and frame.
    uint32_t InitialCandidates = 8;
    bool     TemporalReuse = true;
    // History is clamped to this many times the initial candidate count, which bounds how long a stale sample lives.
    uint32_t MaxHistory = 20;
    uint32_t SpatialSamples = 4;
    float    SpatialRadius = 24.0f;
    // Sums every local light with its own shadow ray instead of resampling; the ground truth for the reservoir path.
    bool     Reference = false;
    bool     operator==(const RestirDISettings&) const = default;
};

// Lighting pass: ReSTIR DI for the point and spot lights, the analytic sun with its denoised shadow, and image-based
// light. See Docs/RestirDI.md.
class RestirDI
{
public:
    static constexpr hz::Format ColorFormat = hz::Format::R32G32B32A32_SFLOAT;
    // Upper bound for the per-frame light table; the reservoir keeps 16 bits for the index.
    static constexpr uint32_t   MaxLights = 256;
    // Frames in flight, matching RenderPasses' frame constant ring.
    static constexpr uint32_t   FrameCount = 3;

    explicit RestirDI(hz::RenderContext& Context);
    // The application waits for the GPU before resizing, so the reservoirs are replaced directly.
    void Resize(uint32_t Width, uint32_t Height);

    void     Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const hz::SceneView& View,
                     const hz::GpuDrawList& Draws, const hz::GPUAccelerationStructure* TLAS, const LightingResources& Resources,
                     const hz::GPUTexture& Color);
    uint32_t GetLightCount() const { return LightCount; }

    RestirDISettings Settings;

private:
    void UploadLights(const hz::SceneView& View);

    hz::RenderContext&                        Context;
    hz::GPUPipeline                           CandidatesPipeline;
    hz::GPUPipeline                           ShadePipeline;
    hz::GPUSampler                            MaterialSampler;
    hz::FixedArray<hz::GPUBuffer, FrameCount> Lights;
    // Two reservoirs alternate between frames: the temporal result of the previous frame and of this one.
    hz::GPUBuffer                             History[2];
    RestirDILight                             LightUpload[MaxLights] = {};
    uint32_t                                  Width = 0;
    uint32_t                                  Height = 0;
    uint32_t                                  LightCount = 0;
    uint32_t                                  FrameNumber = 0;
    uint32_t                                  CurrentHistory = 0;
    RestirDISettings                          PreviousSettings;
    // Set by Resize and settings changes so the next frame starts without history.
    bool                                      ClearHistory = true;
    bool                                      LoggedLights = false;
};
