// ReSTIR DI over the local lights (Bitterli et al. 2020, "Spatiotemporal reservoir resampling for real-time ray tracing
// with dynamic direct lighting"), plus the Cookbook deferred sun and image-based lighting; see ../Cookbook-LICENSE.txt.
//
// CSCandidates draws initial candidates from the light table, merges the reprojected reservoir of the previous frame and
// stores the result as this frame's history. CSShade merges a few neighbouring reservoirs for shading only, traces one
// shadow ray toward the selected light and composes the lit color. Keeping spatial reuse out of the history stops one
// bright sample from spreading through both reuse passes frame after frame. Reuse is the biased variant: neighbours are reweighted by the target function at
// the receiver, without visibility. The sun keeps its analytic evaluation with the denoised ray traced or mapped shadow,
// because its illuminance would win almost every unshadowed resampling and leave shadowed pixels without a sample.
#include "Common.slang"
#include "LightingBRDF.slang"
#include "Math.slang"

cbuffer RootConstant0
{
    uint  frameIndex;
    uint  lightingFrameIndex;
    uint  lightsIndex;
    uint  lightCount;
    uint  emissiveIndex;
    uint  normalMaterialIndex;
    uint  baseColorMetallicIndex;
    uint  depthIndex;
    uint  motionIndex;
    uint  shadowMapIndex;
    uint  shadowSamplerIndex;
    uint  environmentIndex;
    uint  irradianceIndex;
    uint  brdfLutIndex;
    uint  environmentSamplerIndex;
    uint  previousReservoirIndex;
    uint  outputReservoirIndex;
    uint  currentReservoirIndex;
    uint  colorIndex;
    uint  rtShadowAOIndex;
    uint  rtReflectionIndex;
    uint  ssaoIndex;
    uint  tlasIndex;
    uint  drawsIndex;
    uint  materialsIndex;
    uint  samplerIndex;
    uint  width;
    uint  height;
    uint  frameNumber;
    uint  initialCandidates;
    uint  temporalReuse;
    uint  spatialSamples;
    float spatialRadius;
    uint  maxHistory;
    uint  reference;
};

#include "RTTrace.slang"

// Mirrors RestirDILight on the CPU, 64 bytes.
struct RestirDILight
{
    float4 position;  // xyz emitter centre, w range in metres (0 = unbounded)
    float4 direction; // xyz unit direction the emitter faces (local -Z), w cos of the outer cone half angle
    float4 color;     // rgb intensity in candela, w cos of the inner cone half angle
    uint   type;      // hz::LightType
    float  lwidth;
    float  lheight;
    float  area;
};

// Mirrors RestirDIReservoir on the CPU, 16 bytes. The point and spot lights in the table are delta lights, so the light
// index identifies the sample. viewDepth and the material ID describe the receiver and reject stale history.
struct RestirDIReservoir
{
    uint  light;     // light index in the low 16 bits, receiver material ID in the high 16 bits
    float weight;    // W = wSum / (M * pHat(selected))
    float count;     // M
    float viewDepth; // receiver view depth, 0 for an empty reservoir
};

static const uint  kLightTypePoint = 1;
static const uint  kLightTypeSpot = 2;
static const uint  kInvalidLight = 0xffffu;
// Relative view depth difference that still counts as the same surface.
static const float kDepthTolerance = 0.1;
static const float kNormalTolerance = 0.9;

struct RestirDISurface
{
    float3 position;
    float3 normal;
    float3 baseColor;
    float3 f0;
    float  metallic;
    float  roughness;
    float  specular;
    float  viewDepth;
    uint   material;
};

// Stateless PCG hash RNG seeded from the pixel and the frame.
uint restirDIHash(uint value)
{
    const uint state = value * 747796405u + 2891336453u;
    const uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

struct RestirDIRandom
{
    uint state;
};

RestirDIRandom restirDICreateRandom(int2 pixel, uint salt)
{
    RestirDIRandom random;
    random.state = restirDIHash(uint(pixel.x) ^ restirDIHash(uint(pixel.y) ^ restirDIHash(frameNumber * 4u + salt)));
    return random;
}

float restirDINextRandom(inout RestirDIRandom random)
{
    random.state = restirDIHash(random.state);
    return float(random.state >> 8) * (1.0 / 16777216.0);
}

float luminance(float3 color) { return dot(color, float3(0.2126, 0.7152, 0.0722)); }

bool loadSurface(int2 pixel, FrameData frame, out RestirDISurface surface)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[depthIndex];
    const float      depth = depthTexture.Load(int3(pixel, 0));
    surface.position = 0.0;
    surface.normal = 0.0;
    surface.baseColor = 0.0;
    surface.f0 = 0.0;
    surface.metallic = 0.0;
    surface.roughness = 0.0;
    surface.specular = 0.0;
    surface.viewDepth = 0.0;
    surface.material = 0;
    if (depth >= 1.0)
    {
        return false;
    }
    Texture2D<float4> normalMaterial = ResourceDescriptorHeap[normalMaterialIndex];
    Texture2D<float4> baseColorMetallic = ResourceDescriptorHeap[baseColorMetallicIndex];
    Texture2D<float4> motion = ResourceDescriptorHeap[motionIndex];
    const float4      g1 = normalMaterial.Load(int3(pixel, 0));
    const float4      albedo = baseColorMetallic.Load(int3(pixel, 0));
    const float2      properties = unpackMaterialProperties(float2(g1.ba));
    surface.position = reconstructWorldPosition((float2(pixel) + 0.5) / float2(width, height), depth, frame.inverseViewProjection);
    surface.normal = decodeOctNormal(g1.xy);
    surface.baseColor = albedo.rgb;
    surface.metallic = saturate(albedo.a);
    surface.roughness = saturate(properties.x);
    surface.specular = properties.y;
    surface.f0 = pbrF0(surface.baseColor, surface.metallic, surface.specular);
    surface.viewDepth = mul(frame.viewProjection, float4(surface.position, 1.0)).w;
    surface.material = uint(round(motion.Load(int3(pixel, 0)).a * 255.0));
    return true;
}

// Unshadowed reflected radiance f * Le * cos from one light, and the segment toward it for the shadow ray.
float3 evaluateLight(uint index, RestirDISurface surface, float3 v, out float3 direction, out float distance)
{
    StructuredBuffer<RestirDILight> lights = ResourceDescriptorHeap[lightsIndex];
    const RestirDILight             light = lights[index];
    const float3                    offset = light.position.xyz - surface.position;
    distance = length(offset);
    direction = offset / max(distance, 1e-4);
    if (distance < 1e-4 || (light.position.w > 0.0 && distance > light.position.w))
    {
        return 0.0;
    }
    float3 radiance = light.color.rgb / (distance * distance);
    if (light.type == kLightTypeSpot)
    {
        const float cosAngle = dot(-direction, light.direction.xyz);
        if (cosAngle < light.direction.w)
        {
            return 0.0;
        }
        radiance *= smoothstep(light.direction.w, light.color.w, cosAngle);
    }
    return pbrMetallicRoughness(surface.baseColor, surface.metallic, surface.roughness, surface.normal, v, direction, surface.f0) *
           radiance;
}

float targetPdf(uint index, RestirDISurface surface, float3 v)
{
    float3 direction;
    float  distance;
    return luminance(evaluateLight(index, surface, v, direction, distance));
}

RestirDIReservoir emptyReservoir(RestirDISurface surface)
{
    RestirDIReservoir reservoir;
    reservoir.light = kInvalidLight | (surface.material << 16);
    reservoir.weight = 0.0;
    reservoir.count = 0.0;
    reservoir.viewDepth = surface.viewDepth;
    return reservoir;
}

uint reservoirLight(RestirDIReservoir reservoir) { return reservoir.light & 0xffffu; }

// Streaming merge of one weighted sample. Returns true when it replaces the selection.
bool mergeSample(inout RestirDIReservoir reservoir, inout float weightSum, inout float selectedPdf, uint light, float pdf, float weight,
                 float count, inout RestirDIRandom random)
{
    weightSum += weight;
    reservoir.count += count;
    if (weight > 0.0 && restirDINextRandom(random) * weightSum < weight)
    {
        reservoir.light = (reservoir.light & 0xffff0000u) | light;
        selectedPdf = pdf;
        return true;
    }
    return false;
}

void finalizeReservoir(inout RestirDIReservoir reservoir, float weightSum, float selectedPdf)
{
    reservoir.weight = selectedPdf > 0.0 && reservoir.count > 0.0 ? weightSum / (reservoir.count * selectedPdf) : 0.0;
    if (reservoir.weight == 0.0)
    {
        reservoir.light |= kInvalidLight;
    }
}

// Merges another reservoir, reweighted by the target function at this receiver.
void mergeReservoir(inout RestirDIReservoir reservoir, inout float weightSum, inout float selectedPdf, RestirDIReservoir other,
                    float countLimit, RestirDISurface surface, float3 v, inout RestirDIRandom random)
{
    const uint light = reservoirLight(other);
    if (light >= lightCount || other.count <= 0.0)
    {
        return;
    }
    const float count = min(other.count, countLimit);
    const float pdf = targetPdf(light, surface, v);
    mergeSample(reservoir, weightSum, selectedPdf, light, pdf, pdf * other.weight * count, count, random);
}

bool sameSurface(RestirDISurface surface, float viewDepth, uint material)
{
    return viewDepth > 0.0 && abs(viewDepth - surface.viewDepth) <= kDepthTolerance * surface.viewDepth && material == surface.material;
}

RestirDIReservoir loadReservoir(uint buffer, int2 pixel)
{
    StructuredBuffer<RestirDIReservoir> reservoirs = ResourceDescriptorHeap[buffer];
    return reservoirs[pixel.y * width + pixel.x];
}

void storeReservoir(uint buffer, int2 pixel, RestirDIReservoir reservoir)
{
    RWStructuredBuffer<RestirDIReservoir> reservoirs = ResourceDescriptorHeap[buffer];
    reservoirs[pixel.y * width + pixel.x] = reservoir;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void CSCandidates(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x >= width || thread.y >= height)
    {
        return;
    }
    const int2                  pixel = int2(thread.xy);
    StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
    const FrameData             frame = frames[0];
    RestirDISurface             surface;
    if (!loadSurface(pixel, frame, surface) || lightCount == 0 || reference != 0)
    {
        storeReservoir(outputReservoirIndex, pixel, emptyReservoir(surface));
        return;
    }
    const float3      v = normalize(frame.eye.xyz - surface.position);
    RestirDIRandom    random = restirDICreateRandom(pixel, 0);
    RestirDIReservoir reservoir = emptyReservoir(surface);
    float             weightSum = 0.0;
    float             selectedPdf = 0.0;
    // Uniform light selection: each candidate has source pdf 1 / lightCount.
    for (uint i = 0; i < initialCandidates; ++i)
    {
        const uint  light = min(uint(restirDINextRandom(random) * float(lightCount)), lightCount - 1);
        const float pdf = targetPdf(light, surface, v);
        mergeSample(reservoir, weightSum, selectedPdf, light, pdf, pdf * float(lightCount), 1.0, random);
    }
    finalizeReservoir(reservoir, weightSum, selectedPdf);

    if (temporalReuse != 0)
    {
        Texture2D<float4> motion = ResourceDescriptorHeap[motionIndex];
        const float2      uv = (float2(pixel) + 0.5) / float2(width, height);
        const float2      previousUv = uv - decodeMotionVector(motion.Load(int3(pixel, 0)).rgb);
        if (all(previousUv >= 0.0) && all(previousUv < 1.0))
        {
            const RestirDIReservoir previous = loadReservoir(previousReservoirIndex, int2(previousUv * float2(width, height)));
            // The previous view depth of this point, which the reservoir stored when it was that frame's receiver.
            const float previousDepth = mul(frame.previousViewProjection, float4(surface.position, 1.0)).w;
            RestirDISurface previousSurface = surface;
            previousSurface.viewDepth = previousDepth;
            if (sameSurface(previousSurface, previous.viewDepth, previous.light >> 16))
            {
                // Rebuild wSum from the finalized current reservoir, then merge the history clamped to maxHistory * M.
                weightSum = reservoir.weight * reservoir.count * selectedPdf;
                mergeReservoir(reservoir, weightSum, selectedPdf, previous, float(maxHistory) * float(initialCandidates), surface, v,
                               random);
                finalizeReservoir(reservoir, weightSum, selectedPdf);
            }
        }
    }
    storeReservoir(outputReservoirIndex, pixel, reservoir);
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
                             frame.eye.xyz) -
                      viewDistance) /
                      viewDistance;
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

float traceVisibility(RestirDISurface surface, int2 pixel, FrameData frame, float3 direction, float distance)
{
    if (tlasIndex == INVALID_TEXTURE)
    {
        return 1.0;
    }
    Texture2D<float>                depthTexture = ResourceDescriptorHeap[depthIndex];
    const float2                    uv = (float2(pixel) + 0.5) / float2(width, height);
    const float                     depth = depthTexture.Load(int3(pixel, 0));
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[tlasIndex];
    const float3 origin = rtGBufferRayOrigin(uv, depth, surface.position, surface.normal, frame.eye.xyz, frame.inverseViewProjection);
    // Stop short of the light so an emitter mesh at the light position does not occlude itself.
    RayDesc ray = { origin, 0.0, direction, max(distance * 0.999 - 0.01, 0.0) };
    return traceRTShadow(scene, ray, true);
}

// Ground truth for the reservoir path: every local light with its own shadow ray.
float3 referenceLocalLighting(RestirDISurface surface, float3 v, int2 pixel, FrameData frame)
{
    float3 result = 0.0;
    for (uint light = 0; light < lightCount; ++light)
    {
        float3       direction;
        float        distance;
        const float3 contribution = evaluateLight(light, surface, v, direction, distance);
        if (any(contribution > 0.0))
        {
            result += contribution * traceVisibility(surface, pixel, frame, direction, distance);
        }
    }
    return result;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void CSShade(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x >= width || thread.y >= height)
    {
        return;
    }
    const int2                  pixel = int2(thread.xy);
    Texture2D<float4>           emissive = ResourceDescriptorHeap[emissiveIndex];
    RWTexture2D<float4>         color = ResourceDescriptorHeap[colorIndex];
    StructuredBuffer<FrameData> frames = ResourceDescriptorHeap[frameIndex];
    const FrameData             frame = frames[0];
    const float3                scene = emissive.Load(int3(pixel, 0)).rgb;
    RestirDISurface             surface;
    // Only the cleared depth means "no geometry"; the environment pass then shades the sky into those pixels.
    if (!loadSurface(pixel, frame, surface))
    {
        color[pixel] = float4(scene, 1.0);
        return;
    }
    const float3 v = normalize(frame.eye.xyz - surface.position);

    float3 local = 0.0;
    if (reference != 0)
    {
        local = referenceLocalLighting(surface, v, pixel, frame);
    }
    else if (lightCount != 0)
    {
        RestirDIReservoir reservoir = loadReservoir(currentReservoirIndex, pixel);
        if (spatialSamples != 0)
        {
            RestirDIRandom random = restirDICreateRandom(pixel, 1);
            const uint     light = reservoirLight(reservoir);
            float          selectedPdf = light < lightCount ? targetPdf(light, surface, v) : 0.0;
            float          weightSum = reservoir.weight * reservoir.count * selectedPdf;
            Texture2D<float4> normals = ResourceDescriptorHeap[normalMaterialIndex];
            for (uint i = 0; i < spatialSamples; ++i)
            {
                const float  angle = 2.0 * PI * restirDINextRandom(random);
                const float  radius = spatialRadius * sqrt(restirDINextRandom(random));
                const int2   neighbour = pixel + int2(round(float2(cos(angle), sin(angle)) * radius));
                if (any(neighbour < 0) || any(neighbour >= int2(width, height)) || all(neighbour == pixel))
                {
                    continue;
                }
                const RestirDIReservoir other = loadReservoir(currentReservoirIndex, neighbour);
                if (!sameSurface(surface, other.viewDepth, other.light >> 16) ||
                    dot(surface.normal, decodeOctNormal(normals.Load(int3(neighbour, 0)).xy)) < kNormalTolerance)
                {
                    continue;
                }
                mergeReservoir(reservoir, weightSum, selectedPdf, other, other.count, surface, v, random);
            }
            finalizeReservoir(reservoir, weightSum, selectedPdf);
        }
        const uint light = reservoirLight(reservoir);
        if (light < lightCount)
        {
            float3       direction;
            float        distance;
            const float3 contribution = evaluateLight(light, surface, v, direction, distance);
            const float  visibility = traceVisibility(surface, pixel, frame, direction, distance);
            local = contribution * reservoir.weight * visibility;
        }
    }

    // The sun, image-based lighting and ray traced reflections, as in the Cookbook deferred pass.
    StructuredBuffer<LightingFrameData> lightingFrames = ResourceDescriptorHeap[lightingFrameIndex];
    const LightingFrameData             light = lightingFrames[0];
    const LightingBindings              bindings = { shadowMapIndex,     shadowSamplerIndex, environmentIndex,  irradianceIndex,
                                                     brdfLutIndex,       environmentSamplerIndex, rtShadowAOIndex, rtReflectionIndex };
    const RestirDITraced traced = loadTraced(pixel, length(surface.position - frame.eye.xyz), frame);
    const float visibility = rtShadowAOIndex != INVALID_TEXTURE && light.rtShadowEnabled != 0 ? traced.shadowAO.r
                                                                                               : shadowVisibility(surface.position, light, bindings);
    float ao = 1.0;
    if (rtShadowAOIndex != INVALID_TEXTURE && light.rtAOEnabled != 0)
    {
        ao *= pow(saturate(traced.shadowAO.g), light.rtAOPower);
    }
    if (ssaoIndex != INVALID_TEXTURE)
    {
        Texture2D<float4> occlusion = ResourceDescriptorHeap[ssaoIndex];
        ao *= occlusion.Load(int3(pixel, 0)).r;
    }
    PBRLighting lighting = evaluatePBRLighting(surface.baseColor, surface.metallic, surface.roughness, surface.normal, v,
                                               -normalize(light.lightDirection.xyz), visibility, ao, light, bindings, true, surface.specular);
    if (rtReflectionIndex != INVALID_TEXTURE && surface.roughness < kRTReflectionMaxRoughness)
    {
        lighting.indirectSpecular = lerp(lighting.specularWeight * traced.reflection, lighting.indirectSpecular,
                                         smoothstep(kRTReflectionFadeStart, kRTReflectionMaxRoughness, surface.roughness));
    }
    color[pixel] = float4(scene + local + lighting.direct + lighting.indirectDiffuse + lighting.indirectSpecular, 1.0);
}
