#pragma once

#include "Core/IContainer.h"
#include "Core/IMath.h"
#include "Graphics/RenderContext.h"
#include "LightingTypes.h"
#include "Scene/SceneView.h"

// One light as ReSTIR DI samples it, rebuilt from RenderScene every frame and read through the bindless heap.
// Mirrors RestirDILight in RestirDI.slang field for field; both sides must stay 64 bytes.
struct alignas(16) RestirDILight
{
    Vector4  position;  // xyz emitter centre in world space, w range in metres (0 = unbounded).
    Vector4  direction; // xyz unit direction the emitter faces (local -Z), w cos of the outer cone half angle.
    Vector4  color;     // rgb colour * intensity in candela (or lux for Directional), w cos of the inner cone half angle.
    uint32_t type = 0;  // hz::LightType; the reference path evaluates Directional, Point and Spot, area types are stubs.
    float    width = 0; // Rect width, or radius for Disk, Sphere and Cylinder.
    float    height = 0; // Rect height, or length for Cylinder.
    float    area = 0;  // Emitting surface area in m^2 for area lights; zero for delta lights.
};
static_assert(sizeof(RestirDILight) == 64);

// One pixel's light reservoir. The weight and the sampled emitter are kept apart so temporal and spatial
// reuse can combine them without touching the estimator. Mirrors RestirDIReservoir in RestirDI.slang.
// History costs 64 B per pixel per frame in flight, about 400 MB at 1080p and 1.6 GB at 4K; shrink the
// struct once the fields settle.
struct alignas(16) RestirDIReservoir
{
    Vector4  samplePoint;    // xyz world position of the sampled point on the emitter, w target pdf.
    Vector4  sampleNormal;   // xyz emitter normal at the sampled point, w solid-angle pdf of the sample.
    Vector4  sampleRadiance; // rgb incident radiance the sample delivers at the receiver, w reservoir weight W.
    uint32_t lightIndex = 0;
    uint32_t sampleCount = 0; // M, the number of candidates this reservoir represents.
    uint32_t padding0 = 0;
    uint32_t padding1 = 0;
};
static_assert(sizeof(RestirDIReservoir) == 64);

// Deferred lighting replaced by a ReSTIR DI reservoir pass. The resource layer (light buffer, reservoir
// history, G-buffer and environment bindings) is ready; the sampling and reuse stages are stubs that
// return an empty reservoir. See Docs/RestirDI.md for the implementation steps.
class RestirDI
{
public:
    static constexpr hz::Format ColorFormat = hz::Format::R32G32B32A32_SFLOAT;

    explicit RestirDI(hz::RenderContext& Context);
    void Resize(uint32_t Width, uint32_t Height);

    void     Execute(hz::CommandList& Commands, const hz::GPUBuffer& Frame, const DeferredGBuffer& GBuffer, const hz::SceneView& View,
                     const LightingResources& Resources, const hz::GPUTexture& Color);
    uint32_t GetLightCount() const { return LightCount; }

    // Upper bound for the per-frame light table.
    static constexpr uint32_t MaxLights = 256;
    // Frames in flight, matching RenderPasses' frame constant ring.
    static constexpr uint32_t FrameCount = 3;
    // Reservoir history covers those frames: the next frame reads the slot this one writes.
    static constexpr uint32_t ReservoirCount = 3;

private:
    // Fills LightUpload from the active RenderScene lights and stages it into this frame's light table.
    void UploadLights(const hz::SceneView& View);

    hz::RenderContext&                            Context;
    hz::GPUPipeline                               Pipeline;
    hz::FixedArray<hz::GPUBuffer, FrameCount>     Lights;
    hz::FixedArray<hz::GPUBuffer, ReservoirCount> Reservoirs;
    RestirDILight                                 LightUpload[MaxLights] = {};
    uint32_t                                      Width = 0;
    uint32_t                                      Height = 0;
    uint32_t                                      LightCount = 0;
    // Set by Resize so the first recorded frame clears the history buffers that are still undefined.
    bool                                          ClearReservoirs = false;
    bool                                          LoggedLights = false;
};
