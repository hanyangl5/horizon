#include "RTShared.slang"
#include "BlueNoise.slang"
#include "MonteCarlo.slang"
#include "Random.slang"
#include "DeferredLightCommon.slang"

struct RayPayload
{
    float3 Color;
    uint   Depth;
};

[shader("raygeneration")]
void rayGenMain()
{
    RWTexture2D<float4>         Output = ResourceDescriptorHeap[OutputIndex];
    Texture2D<float>            DepthTexture = ResourceDescriptorHeap[DepthIndex];
    Texture2D<float4>           Normals = ResourceDescriptorHeap[NormalIndex];
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[FrameIndexBuffer];
    uint                        Width;
    uint                        Height;
    DepthTexture.GetDimensions(Width, Height);

    uint2  Pixel = DispatchRaysIndex().xy;
    uint2  Size = DispatchRaysDimensions().xy;
    // Load depth and normal from the same full-resolution pixel, without blending across silhouettes.
    int2   DepthSize = int2(Width, Height);
    int2   SourcePixel = RTSourcePixel(int2(Pixel), int2(Size), DepthSize);
    float  Depth = DepthTexture.Load(int3(SourcePixel, 0));
    if (Depth >= 1.0 || TlasIndex == INVALID_TEXTURE)
    {
        Output[Pixel] = float4(0.0);
        return;
    }
    RaytracingAccelerationStructure Scene = ResourceDescriptorHeap[TlasIndex];
    float2                          SourceUV = (float2(SourcePixel) + 0.5) / float2(DepthSize);
    float4                          G1 = Normals.Load(int3(SourcePixel, 0));
    float3                          N = DecodeOctNormal(G1.xy);
    float2                          Properties = UnpackMaterialProperties(float2(G1.ba));
    float3                          WorldPos = ReconstructWorldPosition(SourceUV, Depth, Frame[0].InverseViewProjection);
    float3                          ViewDir = normalize(Frame[0].Eye.xyz - WorldPos);
    if (Properties.x >= RTReflectionMaxRoughness)
    {
        Output[Pixel] = float4(0.0);
        return;
    }
    const float Roughness = Properties.x;
    float3 Direction = reflect(-ViewDir, N);
    float Weight = 1.0;
    if (Roughness > 1.0 / 255.0)
    {
        const float3x3 Basis = GetTangentBasis(N);
        const float3 V = mul(Basis, ViewDir);
        if (V.z <= 0.0)
        {
            Output[Pixel] = float4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        float2 Random;
        if (BlueNoiseIndex != INVALID_TEXTURE)
        {
            Texture2D<float2> BlueNoise = ResourceDescriptorHeap[BlueNoiseIndex];
            Random = clamp(GetBlueNoiseVec2(BlueNoise, Pixel, FrameNumber), 0.5 / 65536.0, 1.0 - 0.5 / 65536.0);
        }
        else
        {
            Random = (float2(Rand3DPCG16(int3(Pixel, FrameNumber)).xy) + 0.5) / 65536.0;
        }
        const float Alpha = max(Pow2(Roughness), sqrt(2e-6));
        const float4 Sample = ImportanceSampleVisibleGGX(Random, Alpha, V);
        const float3 L = reflect(-V, Sample.xyz);
        if (L.z <= 0.0)
        {
            // A below-horizon direction contributes zero, but still counts as a sample.
            Output[Pixel] = float4(0.0, 0.0, 0.0, 1.0);
            return;
        }
        Direction = normalize(mul(L, Basis));

        // GGX BRDF*cos / VNDF PDF with Fresnel factored out. Normalize by directional albedo so
        // the existing deferred split-sum weight is applied exactly once to the incoming radiance.
        const float LenV = length(float3(Alpha * V.xy, V.z));
        const float LenL = length(float3(Alpha * L.xy, L.z));
        Weight = L.z * (V.z + LenV) / (L.z * LenV + V.z * LenL);
        Weight /= GGXEnergyLookup(Roughness, saturate(V.z)).x;
    }

    RayDesc Ray = { RTGBufferRayOrigin(SourceUV, Depth, WorldPos, N, Frame[0].Eye.xyz, Frame[0].InverseViewProjection), 0.0,
                    Direction, 1e6 };
    RayPayload Payload = {};
    TraceRay(Scene, RAY_FLAG_NONE, 0xff, 0, 0, 0, Ray, Payload);
    Output[Pixel] = float4(Payload.Color * Weight, 1.0);
}

[shader("miss")]
void missMain(inout RayPayload Payload)
{
    Payload.Color = 0;
    Payload.Depth = 0xff;
    if (EnvironmentIndex == INVALID_TEXTURE || EnvironmentSamplerIndex == INVALID_TEXTURE)
    {
        return;
    }
    StructuredBuffer<LightingFrameData> Light = ResourceDescriptorHeap[LightingIndex];
    TextureCube<float4>                 Environment = ResourceDescriptorHeap[EnvironmentIndex];
    SamplerState                        Sampler = SamplerDescriptorHeap[EnvironmentSamplerIndex];
    float3                              Direction = WorldRayDirection();
    Payload.Color =
        Environment.SampleLevel(Sampler, float3(-Direction.x, Direction.y, -Direction.z), 0).rgb * max(Light[0].EnvironmentIntensity, 0.0) * Light[0].EnvironmentTint.rgb;
}

[shader("closesthit")]
void closestHitMain(inout RayPayload Payload, BuiltInTriangleIntersectionAttributes Attributes)
{
    GpuDrawData Draw = LoadRTDraw(InstanceID());
    uint3             Indices = LoadRTTriangle(Draw, PrimitiveIndex());
    float3            Bary = float3(1.0 - Attributes.barycentrics.x - Attributes.barycentrics.y, Attributes.barycentrics);
    float3            V = -normalize(WorldRayDirection());
    ByteAddressBuffer Positions = ResourceDescriptorHeap[NonUniformResourceIndex(Draw.Positions)];
    const float3      P0 = asfloat(Positions.Load3(Indices.x * sizeof(float3)));
    const float3      P1 = asfloat(Positions.Load3(Indices.y * sizeof(float3)));
    const float3      P2 = asfloat(Positions.Load3(Indices.z * sizeof(float3)));
    float3            GeometricNormal = normalize(mul(cross(P1 - P0, P2 - P0), (float3x3)WorldToObject3x4()));
    GeometricNormal = dot(GeometricNormal, V) < 0.0 ? -GeometricNormal : GeometricNormal;
    // Meshes without a normal stream shade with the geometric normal.
    float3 N = GeometricNormal;
    if (Draw.Normals != INVALID_TEXTURE)
    {
        ByteAddressBuffer Normals = ResourceDescriptorHeap[NonUniformResourceIndex(Draw.Normals)];
        const float3      Normal = DecodeVertexNormal(Normals.Load(Indices.x * 4)) * Bary.x +
                              DecodeVertexNormal(Normals.Load(Indices.y * 4)) * Bary.y + DecodeVertexNormal(Normals.Load(Indices.z * 4)) * Bary.z;
        N = normalize(mul(Normal, (float3x3)WorldToObject3x4()));
        N = dot(N, V) < 0.0 ? -N : N;
    }
    const float3 HitPosition =
        RTOffsetHitPosition(mul(ObjectToWorld3x4(), float4(P0 * Bary.x + P1 * Bary.y + P2 * Bary.z, 1.0)), GeometricNormal);
    float2            UV = InterpolateRTUV(Draw, Indices, Attributes.barycentrics);
    ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
    GpuMaterialData   Material = Materials.Load<GpuMaterialData>(Draw.Material * sizeof(GpuMaterialData));
    SamplerState      Sampler = SamplerDescriptorHeap[SamplerIndex];
    float3            BaseColor = Material.BaseColorFactor.rgb;
    if (Material.BaseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Texture = ResourceDescriptorHeap[NonUniformResourceIndex(Material.BaseColorTexture)];
        BaseColor *= Texture.SampleLevel(Sampler, UV, 0).rgb;
    }
    float Metallic = Material.MetallicFactor;
    float Roughness = Material.RoughnessFactor;
    if (Material.MetallicRoughnessTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Texture = ResourceDescriptorHeap[NonUniformResourceIndex(Material.MetallicRoughnessTexture)];
        float4            Mr = Texture.SampleLevel(Sampler, UV, 0);
        Metallic *= Mr.b;
        Roughness *= Mr.g;
    }
    float3 Emissive = Material.EmissiveFactor;
    if (Material.EmissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> Texture = ResourceDescriptorHeap[NonUniformResourceIndex(Material.EmissiveTexture)];
        Emissive *= Texture.SampleLevel(Sampler, UV, 0).rgb;
    }
    StructuredBuffer<LightingFrameData> Light = ResourceDescriptorHeap[LightingIndex];
    float3                              L = -normalize(Light[0].LightDirection.xyz);
    float                               Visibility = 1.0;
    if (ShadowsEnabled != 0)
    {
        RaytracingAccelerationStructure Scene = ResourceDescriptorHeap[TlasIndex];
        RayDesc                         Ray = { HitPosition, 0.0, L, 1e4 };
        Visibility = dot(GeometricNormal, L) > 0.0 ? TraceRTShadow(Scene, Ray, AlphaTestMethod != 0) : 0.0;
    }
    const FGBufferData Surface = MakeDefaultLitMaterial(N, BaseColor, Metallic, Roughness, 0.5);
    const FIndirectLighting Indirect = EvaluateIndirectLighting(Surface.DiffuseColor, Surface.SpecularColor, Surface.Roughness, N, V,
                                                                 1.0, Light[0], RTLightingBindings(), false);
    // One bounce: the sun is shadowed by an inline query, without recursive TraceRay.
    Payload.Color = Emissive + Indirect.Diffuse + AccumulateSceneLights(Light[0], Surface, HitPosition, V, Visibility, 2u);
}

// Only masked geometry invokes Any-Hit; opaque geometry is accepted by traversal.
[shader("anyhit")]
void anyHitFoliage(inout RayPayload Payload, in BuiltInTriangleIntersectionAttributes Attribs)
{
    GpuDrawData Mesh = LoadRTDraw(InstanceID());
    ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
    GpuMaterialData   Material = Materials.Load<GpuMaterialData>(Mesh.Material * sizeof(GpuMaterialData));
    float             Alpha = SampleRayTracingAlpha(Mesh, Material, PrimitiveIndex(), Attribs.barycentrics);
    if (Alpha < Material.AlphaCutoff)
    {
        IgnoreHit();
    }
}
