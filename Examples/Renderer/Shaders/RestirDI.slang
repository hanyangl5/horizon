// Cookbook deferred lighting adapted to Horizon's GBuffer and turned into a ReSTIR DI scaffold;
// see ../Cookbook-LICENSE.txt.
//
// The resource layer below is complete: bindings, the per-frame light table, the reservoir history and the
// G-buffer reader. The four ReSTIR DI stages are stubs; see Docs/RestirDI.md for the implementation steps.
#include "Common.slang"
#include "LightingBRDF.slang"
#include "Math.slang"

cbuffer RootConstant0
{
    uint frameIndex;
    uint lightingFrameIndex;
    uint lightsIndex;
    uint lightCount;
    uint emissiveIndex;
    uint normalMaterialIndex;
    uint baseColorMetallicIndex;
    uint depthIndex;
    uint motionIndex;
    uint shadowMapIndex;
    uint shadowSamplerIndex;
    uint environmentIndex;
    uint irradianceIndex;
    uint brdfLutIndex;
    uint environmentSamplerIndex;
    uint previousReservoirIndex;
    uint outputReservoirIndex;
    uint colorIndex;
    uint rtShadowAOIndex;
    uint rtReflectionIndex;
    uint ssaoIndex;
    uint width;
    uint height;
};

// Mirrors RestirDILight on the CPU. Sampled emitters are stored per frame by RestirDI::UploadLights.
struct RestirDILight
{
    float4 position;  // xyz emitter centre, w range in metres (0 = unbounded)
    float4 direction; // xyz unit direction the emitter faces (local -Z), w cos of the outer cone half angle
    float4 color;     // rgb colour * intensity (candela; lux for Directional), w cos of the inner cone half angle
    uint   type;      // hz::LightType
    float  lwidth;    // Rect width, or radius for Disk, Sphere and Cylinder
    float  lheight;   // Rect height, or length for Cylinder
    float  area;      // emitting surface area in m^2, zero for delta lights
};

// Mirrors RestirDIReservoir on the CPU, 64 bytes. One entry per pixel; the history is a ring of three
// buffers so the previous frame's reservoir is still resident while the next one is written.
struct RestirDIReservoir
{
    float4 samplePoint;    // xyz world position of the sampled point on the emitter, w target pdf
    float4 sampleNormal;   // xyz emitter normal at the sampled point, w solid-angle pdf of the sample
    float4 sampleRadiance; // rgb incident radiance at the receiver, w reservoir weight W
    uint   lightIndex;
    uint   sampleCount; // M
    uint2  padding;
};

struct RestirDISurface
{
    float3 position;
    float3 normal;
    float3 baseColor;
    float  metallic;
    float  roughness;
    float  specular;
    float  depth;
};

struct RestirDILightSample
{
    float3 direction; // unit direction from the receiver toward the sampled point
    float3 radiance;  // incident radiance at the receiver (irradiance for Directional)
    float  pdf;       // solid-angle pdf of the sample
    float  distance;
};

static const uint kLightTypeDirectional = 0;
static const uint kLightTypePoint = 1;
static const uint kLightTypeSpot = 2;

// Small stateless hash RNG. Sampling and reuse draw from it, so every candidate of a pixel is reproducible
// from the pixel coordinate and the frame index.
struct RestirDIRandom
{
    uint state;
};

uint restirDIHash(uint value)
{
    const uint state = value * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

RestirDIRandom restirDICreateRandom(int2 pixel, uint frame)
{
    RestirDIRandom random;
    random.state = restirDIHash((uint)pixel.x | ((uint)pixel.y << 16) ^ (frame * 0x9e3779b9u));
    return random;
}

float restirDINextRandom(inout RestirDIRandom random)
{
    random.state = restirDIHash(random.state);
    return float(random.state) * (1.0 / 4294967296.0);
}

// ---------------------------------------------------------------------------------------------
// Resource layer: G-buffer, lights and reservoir storage. Implemented; extend the light reader when a
// new emitter type is sampled.
// ---------------------------------------------------------------------------------------------

RestirDISurface loadSurface(int3 pixel, float depth, float4x4 inverseViewProjection)
{
    Texture2D<float4> normalMaterial = ResourceDescriptorHeap[normalMaterialIndex];
    Texture2D<float4> baseColorMetallic = ResourceDescriptorHeap[baseColorMetallicIndex];
    const float4      g1 = normalMaterial.Load(pixel);
    const float4      surface = baseColorMetallic.Load(pixel);
    const float2      properties = unpackMaterialProperties(float2(g1.ba));
    RestirDISurface   result;
    result.position = reconstructWorldPosition((float2(pixel.xy) + float2(0.5)) / float2(width, height), depth,
                                               inverseViewProjection);
    result.normal = decodeOctNormal(float2(g1.xy));
    result.baseColor = surface.rgb;
    result.metallic = saturate(surface.a);
    result.roughness = saturate(properties.x);
    result.specular = properties.y;
    result.depth = depth;
    return result;
}

// Returns the screen-space motion vector in UV units; previousUV = uv - motion.
float2 loadMotion(int3 pixel, out float materialId)
{
    Texture2D<float4> motionTexture = ResourceDescriptorHeap[motionIndex];
    const float4      encoded = motionTexture.Load(pixel);
    materialId = encoded.a;
    return decodeMotionVector(encoded.rgb);
}

LightingBindings makeLightingBindings()
{
    LightingBindings bindings = { shadowMapIndex, shadowSamplerIndex,      environmentIndex, irradianceIndex,
                                  brdfLutIndex,   environmentSamplerIndex, rtShadowAOIndex,  rtReflectionIndex };
    return bindings;
}

// Depth-aware upsample of the reduced-resolution ray traced results: bilinear weights over the four nearest texels,
// each scaled down by its source pixel's relative view distance difference, so the results do not bleed across edges.
static const float kRTUpsampleDepthScale = 50.0;
struct RTUpsample
{
    int2  taps[4];
    float weights[4];
};
RTUpsample rtUpsample(int2 pixel, int2 size, int2 lowSize, float viewDistance, Texture2D<float> depthTexture, FrameData frame)
{
    const float2 position = float2(pixel) * float2(lowSize) / float2(size);
    const int2   base = int2(floor(position));
    const float2 fraction = position - float2(base);
    RTUpsample   result;
    float        weightSum = 0.0;
    float        closest = 1e30;
    int          closestTap = 0;
    for (int i = 0; i < 4; ++i)
    {
        const int2   offset = int2(i & 1, i >> 1);
        const int2   tap = min(base + offset, lowSize - 1);
        const int2   source = rtSourcePixel(tap, lowSize, size);
        const float  tapDepth = depthTexture.Load(int3(source, 0));
        const float2 bilinear = lerp(1.0 - fraction, fraction, float2(offset));
        const float  difference =
            tapDepth >= 1.0
                ? 1e30
                : abs(length(reconstructWorldPosition((float2(source) + 0.5) / float2(size), tapDepth, frame.inverseViewProjection) -
                             frame.eye.xyz) - viewDistance) / viewDistance;
        result.taps[i] = tap;
        result.weights[i] = bilinear.x * bilinear.y * exp(-difference * kRTUpsampleDepthScale);
        weightSum += result.weights[i];
        if (difference < closest)
        {
            closest = difference;
            closestTap = i;
        }
    }
    for (int i = 0; i < 4; ++i)
    {
        // Thin geometry may match none of the texels; it takes the nearest in depth instead of a blend of the background.
        result.weights[i] = weightSum > 1e-4 ? result.weights[i] / weightSum : float(i == closestTap);
    }
    return result;
}

// The ray traced results at this pixel, upsampled from their reduced resolution.
struct RestirDITraced
{
    float2 shadowAO;
    float3 reflection;
};

RestirDITraced loadTraced(int2 pixel, float viewDistance, FrameData frame)
{
    RestirDITraced traced;
    traced.shadowAO = 1.0;
    traced.reflection = 0.0;
    if (rtShadowAOIndex == INVALID_TEXTURE && rtReflectionIndex == INVALID_TEXTURE)
    {
        return traced;
    }
    Texture2D<float>  depthTexture = ResourceDescriptorHeap[depthIndex];
    Texture2D<float4> anyTraced = ResourceDescriptorHeap[rtShadowAOIndex != INVALID_TEXTURE ? rtShadowAOIndex : rtReflectionIndex];
    int2              lowSize;
    anyTraced.GetDimensions(lowSize.x, lowSize.y);
    const RTUpsample upsample = rtUpsample(pixel, int2(width, height), lowSize, viewDistance, depthTexture, frame);
    if (rtShadowAOIndex != INVALID_TEXTURE)
    {
        Texture2D<float2> shadowAO = ResourceDescriptorHeap[rtShadowAOIndex];
        traced.shadowAO = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            traced.shadowAO += shadowAO.Load(int3(upsample.taps[i], 0)) * upsample.weights[i];
        }
    }
    if (rtReflectionIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> reflection = ResourceDescriptorHeap[rtReflectionIndex];
        for (int i = 0; i < 4; ++i)
        {
            traced.reflection += reflection.Load(int3(upsample.taps[i], 0)).rgb * upsample.weights[i];
        }
    }
    return traced;
}

// Visibility of the directional light. Local-light visibility needs shadow rays and is left to the pass
// implementation; the reference path lights local emitters unshadowed.
float evaluateShadowVisibility(float3 worldPosition, RestirDITraced traced, LightingFrameData light, LightingBindings bindings)
{
    if (rtShadowAOIndex != INVALID_TEXTURE && light.rtShadowEnabled != 0)
    {
        return traced.shadowAO.r;
    }
    return shadowVisibility(worldPosition, light, bindings);
}

float evaluateAmbientOcclusion(int3 pixel, RestirDITraced traced, LightingFrameData light, LightingBindings bindings)
{
    float ao = 1.0;
    if (rtShadowAOIndex != INVALID_TEXTURE && light.rtAOEnabled != 0)
    {
        ao *= pow(saturate(traced.shadowAO.g), light.rtAOPower);
    }
    if (ssaoIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> occlusion = ResourceDescriptorHeap[ssaoIndex];
        ao *= occlusion.Load(pixel).r;
    }
    return ao;
}

RestirDIReservoir emptyReservoir()
{
    RestirDIReservoir reservoir;
    reservoir.samplePoint = 0;
    reservoir.sampleNormal = 0;
    reservoir.sampleRadiance = 0;
    reservoir.lightIndex = 0;
    reservoir.sampleCount = 0;
    reservoir.padding = 0;
    return reservoir;
}

RestirDIReservoir loadReservoir(int3 pixel, uint bufferIndex)
{
    StructuredBuffer<RestirDIReservoir> history = ResourceDescriptorHeap[bufferIndex];
    return history[pixel.y * width + pixel.x];
}

void storeReservoir(RWStructuredBuffer<RestirDIReservoir> output, int3 pixel, RestirDIReservoir reservoir)
{
    output[pixel.y * width + pixel.x] = reservoir;
}

// ---------------------------------------------------------------------------------------------
// ReSTIR DI algorithm: four stages to implement. Each signature already receives every resource the
// stage needs; Docs/RestirDI.md gives the formulas, the order and the validation plan.
// ---------------------------------------------------------------------------------------------

// Step 1. Draw M candidates from the light table into one reservoir, then finalize W = wSum / (M * targetPdf).
RestirDIReservoir initialCandidates(int3 pixel, RestirDISurface surface, float3 v, LightingBindings bindings,
                                    inout RestirDIRandom random)
{
    RestirDIReservoir reservoir = emptyReservoir();
    // TODO(restir-di): sample lightCount lights, build a RestirDILightSample for each, accumulate the
    // streaming weight and store the selected emitter in the reservoir fields.
    return reservoir;
}

// Step 2. Reproject the previous frame's reservoir with the motion vector and merge it with the candidate.
void temporalReuse(inout RestirDIReservoir reservoir, int3 pixel, RestirDISurface surface, float3 v, float2 motion,
                   LightingBindings bindings)
{
    // TODO(restir-di): rebuild the previous UV, reject invalid history, re-evaluate the stored emitter at
    // this pixel and merge it with the bias-corrected temporal weight.
}

// Step 3. Merge the reservoirs of a few neighbouring pixels.
void spatialReuse(inout RestirDIReservoir reservoir, int3 pixel, RestirDISurface surface, float3 v, LightingBindings bindings,
                  inout RestirDIRandom random)
{
    // TODO(restir-di): sample neighbours, re-evaluate their emitters against this pixel, merge with
    // pairwise MIS weights and finalize the reservoir.
}

// Step 4. Shade the selected emitter. Until this returns the reservoir's contribution the pass uses the
// reference path below, so the frame stays lit.
float3 shadeReservoir(RestirDIReservoir reservoir, RestirDISurface surface, float3 v, LightingFrameData light,
                      LightingBindings bindings, RestirDITraced traced)
{
    // TODO(restir-di): return reservoir.sampleRadiance.rgb * reservoir.sampleRadiance.w, or re-evaluate the
    // stored emitter with its BRDF and visibility here.
    return float3(0.0, 0.0, 0.0);
}

// ---------------------------------------------------------------------------------------------
// Reference shading: brute-force direct lighting over the light table plus image-based indirect light.
// It is the ground truth the reservoir path is compared against and keeps the pass useful meanwhile.
// ---------------------------------------------------------------------------------------------

float3 referenceDirectLighting(RestirDISurface surface, float3 v, LightingFrameData lightingFrame, LightingBindings bindings,
                               RestirDITraced traced)
{
    StructuredBuffer<RestirDILight> lights = ResourceDescriptorHeap[lightsIndex];
    const float3                    f0 = pbrF0(surface.baseColor, surface.metallic, surface.specular);
    float3                          result = float3(0.0, 0.0, 0.0);
    for (uint index = 0; index < lightCount; ++index)
    {
        const RestirDILight light = lights[index];
        float3              toLight;
        float3              radiance;
        if (light.type == kLightTypeDirectional)
        {
            toLight = -normalize(light.direction.xyz);
            radiance = light.color.rgb;
        }
        else
        {
            const float3 offset = light.position.xyz - surface.position;
            const float  distance = length(offset);
            if (distance < 1e-4)
            {
                continue;
            }
            toLight = offset / distance;
            radiance = light.color.rgb / (distance * distance);
            if (light.position.w > 0.0 && distance > light.position.w)
            {
                continue;
            }
            if (light.type == kLightTypeSpot)
            {
                const float cosAngle = dot(-toLight, light.direction.xyz);
                if (cosAngle < light.direction.w)
                {
                    continue;
                }
                radiance *= smoothstep(light.direction.w, light.color.w, cosAngle);
            }
        }
        if (dot(surface.normal, toLight) <= 0.0)
        {
            continue;
        }
        const float visibility =
            light.type == kLightTypeDirectional ? evaluateShadowVisibility(surface.position, traced, lightingFrame, bindings) : 1.0;
        result += pbrMetallicRoughness(surface.baseColor, surface.metallic, surface.roughness, surface.normal, v, toLight, f0) *
                  radiance * visibility;
    }
    return result;
}

float3 evaluateIndirectLighting(RestirDISurface surface, float3 v, float ao, LightingFrameData light, LightingBindings bindings,
                                RestirDITraced traced)
{
    if (bindings.environmentSampler == INVALID_TEXTURE)
    {
        return float3(0.0, 0.0, 0.0);
    }
    const float3 f0 = pbrF0(surface.baseColor, surface.metallic, surface.specular);
    const float3 fresnel = pbrFresnel(f0, saturate(dot(surface.normal, v)));
    SamplerState sampler = SamplerDescriptorHeap[bindings.environmentSampler];
    const float  intensity = max(light.environmentIntensity, 0.0);
    float3       result = float3(0.0, 0.0, 0.0);
    if (bindings.irradiance != INVALID_TEXTURE)
    {
        TextureCube<float4> irradiance = ResourceDescriptorHeap[bindings.irradiance];
        const float3        sky = float3(-surface.normal.x, surface.normal.y, -surface.normal.z);
        const float3        diffuseEnvironment = irradiance.SampleLevel(sampler, sky, 0).rgb;
        result += pbrIndirectDiffuse(surface.baseColor, surface.metallic, fresnel, diffuseEnvironment, ao) * intensity;
    }
    if (bindings.environment != INVALID_TEXTURE && bindings.brdfLut != INVALID_TEXTURE)
    {
        TextureCube<float4> environment = ResourceDescriptorHeap[bindings.environment];
        Texture2D<float4>   brdfLut = ResourceDescriptorHeap[bindings.brdfLut];
        const float3        r = reflect(-v, surface.normal);
        const float         lod = surface.roughness * float(max(light.environmentMipCount, 1u) - 1u);
        const float3        radiance = environment.SampleLevel(sampler, float3(-r.x, r.y, -r.z), lod).rgb;
        const float2        brdf =
            brdfLut.SampleLevel(sampler, float2(saturate(dot(surface.normal, v)), surface.roughness), 0).rg;
        const float3 specularWeight = f0 * brdf.x + float3(brdf.y);
        result += specularWeight * radiance * intensity;
        if (rtReflectionIndex != INVALID_TEXTURE && surface.roughness < kRTReflectionMaxRoughness)
        {
            result += lerp(specularWeight * traced.reflection, specularWeight * radiance,
                           smoothstep(kRTReflectionFadeStart, kRTReflectionMaxRoughness, surface.roughness));
        }
    }
    return result;
}

// Flip to true once the four stages above are implemented.
static const bool kUseReSTIRDI = false;

[numthreads(8, 8, 1)]
void CSMain(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x >= width || threadId.y >= height)
    {
        return;
    }
    const int3          pixel = int3(threadId.xy, 0);
    Texture2D<float>    depthTexture = ResourceDescriptorHeap[depthIndex];
    Texture2D<float4>   emissiveTexture = ResourceDescriptorHeap[emissiveIndex];
    RWTexture2D<float4> colorTexture = ResourceDescriptorHeap[colorIndex];
    const float         depth = depthTexture.Load(pixel);
    const float3        scene = emissiveTexture.Load(pixel).rgb;

    // The depth target clears to 1.0 and the projection maps the far plane there, so only the cleared value
    // means "no geometry". The environment pass then shades the sky into those pixels.
    if (depth >= 1.0)
    {
        colorTexture[pixel.xy] = float4(scene, 1.0);
        return;
    }

    StructuredBuffer<FrameData>         frameBuffer = ResourceDescriptorHeap[frameIndex];
    StructuredBuffer<LightingFrameData> lightingBuffer = ResourceDescriptorHeap[lightingFrameIndex];
    const FrameData                     frame = frameBuffer[0];
    const LightingFrameData             light = lightingBuffer[0];
    const RestirDISurface               surface = loadSurface(pixel, depth, frame.inverseViewProjection);
    const float3                        v = normalize(frame.eye.xyz - surface.position);
    const LightingBindings              bindings = makeLightingBindings();
    const RestirDITraced                traced = loadTraced(pixel.xy, length(surface.position - frame.eye.xyz), frame);
    const float                         ao = evaluateAmbientOcclusion(pixel, traced, light, bindings);

    if (kUseReSTIRDI)
    {
        RestirDIRandom    random = restirDICreateRandom(pixel.xy, light.frameIndex);
        RestirDIReservoir reservoir = initialCandidates(pixel, surface, v, bindings, random);
        float             materialId = 0.0;
        const float2      motion = loadMotion(pixel, materialId);
        temporalReuse(reservoir, pixel, surface, v, motion, bindings);
        spatialReuse(reservoir, pixel, surface, v, bindings, random);
        RWStructuredBuffer<RestirDIReservoir> output = ResourceDescriptorHeap[outputReservoirIndex];
        storeReservoir(output, pixel, reservoir);
        colorTexture[pixel.xy] = float4(scene + shadeReservoir(reservoir, surface, v, light, bindings, traced) +
                                            evaluateIndirectLighting(surface, v, ao, light, bindings, traced),
                                        1.0);
        return;
    }

    colorTexture[pixel.xy] = float4(scene + referenceDirectLighting(surface, v, light, bindings, traced) +
                                        evaluateIndirectLighting(surface, v, ao, light, bindings, traced),
                                    1.0);
}
