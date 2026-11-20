#include "Common.slang"
#include "SceneData.slang"

cbuffer RootConstant0
{
    uint  InstanceIndex;
    uint  MaterialIndex;
    uint  MaterialId;
    uint  FrameIndex;
    uint  InstancesIndex;
    uint  MaterialsIndex;
    uint  TexturesIndex;
    uint  SamplerIndex;
    uint  PositionsIndex;
    uint  NormalsIndex;
    uint  TexcoordsIndex;
    uint  DrawsIndex;
    uint  OitHeadsIndex;
    uint  OitCounterIndex;
    uint  OitNodesIndex;
    uint  OitMaxNodes;
    uint  LightingFrameIndex;
    uint  ShadowMapIndex;
    uint  ShadowSamplerIndex;
    uint  EnvironmentIndex;
    uint  IrradianceIndex;
    uint  BrdfLutIndex;
    uint  EnvironmentSamplerIndex;
    uint  OitOpaqueIndex;
    float OitOpacityBoost;
    uint  OitShowHeatmap;
    uint  OitOutputIndex;
    float OitTime;
};

#include "Material.slang"

float4x4 LoadMatrix(ByteAddressBuffer Buffer, uint Offset)
{
    // Runtime matrices store four columns; construct then transpose HLSL's row vectors.
    return transpose(float4x4(asfloat(Buffer.Load4(Offset)), asfloat(Buffer.Load4(Offset + 16)), asfloat(Buffer.Load4(Offset + 32)),
                              asfloat(Buffer.Load4(Offset + 48))));
}

float3 TransformNormal(float4x4 World, float3 Normal)
{
    const float3 X = float3(World[0][0], World[1][0], World[2][0]);
    const float3 Y = float3(World[0][1], World[1][1], World[2][1]);
    const float3 Z = float3(World[0][2], World[1][2], World[2][2]);
    const float3 Cofactor = cross(Y, Z) * Normal.x + cross(Z, X) * Normal.y + cross(X, Y) * Normal.z;
    return normalize(Cofactor * (dot(X, cross(Y, Z)) < 0.0 ? -1.0 : 1.0));
}

struct PSInput
{
    float4                Position: SV_Position;
    float3                WorldPosition: TEXCOORD0;
    float3                Normal: TEXCOORD1;
    float2                Uv: TEXCOORD2;
    float4                CurrentClip: TEXCOORD3;
    float4                PreviousClip: TEXCOORD4;
    nointerpolation uint2 MaterialData: TEXCOORD5;
};

struct PSOutput
{
    float4 Emissive: SV_Target0;
    float4 NormalMaterial: SV_Target1;
    float4 BaseColorMetallic: SV_Target2;
    float4 MotionMaterialId: SV_Target3;
};

PSInput BuildVertex(uint VertexId, uint Instance, uint Material, uint ID, uint PositionBuffer, uint NormalBuffer, uint UvBuffer)
{
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[FrameIndex];
    ByteAddressBuffer           Instances = ResourceDescriptorHeap[InstancesIndex];
    ByteAddressBuffer           Positions = ResourceDescriptorHeap[NonUniformResourceIndex(PositionBuffer)];
    float3                      Position = asfloat(Positions.Load3(VertexId * sizeof(float3)));
    // Meshes without texture coordinates have no stream; they sample texel (0, 0).
    uint                        Uv = 0;
    if (UvBuffer != INVALID_TEXTURE)
    {
        ByteAddressBuffer Texcoords = ResourceDescriptorHeap[NonUniformResourceIndex(UvBuffer)];
        Uv = Texcoords.Load(VertexId * 4);
    }
    const float4x4              World = LoadMatrix(Instances, Instance * 192 + 32);
    const float4x4              PreviousWorld = LoadMatrix(Instances, Instance * 192 + 96);
    float4                      WorldPosition = mul(World, float4(Position, 1.0));
    PSInput                     Output;
    Output.CurrentClip = mul(Frame[0].ViewProjection, WorldPosition);
    Output.PreviousClip = mul(Frame[0].PreviousViewProjection, mul(PreviousWorld, float4(Position, 1.0)));
    Output.Position = Output.CurrentClip;
    Output.WorldPosition = WorldPosition.xyz;
    // Meshes without a normal stream pass a zero normal; SurfaceNormal shades them with the face normal.
    Output.Normal = 0.0;
    if (NormalBuffer != INVALID_TEXTURE)
    {
        ByteAddressBuffer Normals = ResourceDescriptorHeap[NonUniformResourceIndex(NormalBuffer)];
        Output.Normal = TransformNormal(World, DecodeVertexNormal(Normals.Load(VertexId * 4)));
    }
    Output.Uv = float2(f16tof32(Uv & 0xffffu), f16tof32(Uv >> 16));
    Output.MaterialData = uint2(Material, ID);
    return Output;
}

float3 SurfaceNormal(float3 Normal, float3 WorldPosition)
{
    if (dot(Normal, Normal) > 0.0)
    {
        return normalize(Normal);
    }
    StructuredBuffer<FrameData> Frame = ResourceDescriptorHeap[FrameIndex];
    const float3                Face = normalize(cross(ddx(WorldPosition), ddy(WorldPosition)));
    return dot(Face, Frame[0].Eye.xyz - WorldPosition) < 0.0 ? -Face : Face;
}

PSInput VSMain(uint VertexId : SV_VertexID)
{
    return BuildVertex(VertexId, InstanceIndex, MaterialIndex, MaterialId, PositionsIndex, NormalsIndex, TexcoordsIndex);
}

PSInput VSIndirect(uint VertexId : SV_VertexID)
{
    const uint        DrawId = InstanceIndex;
    ByteAddressBuffer Draws = ResourceDescriptorHeap[DrawsIndex];
    uint4             Object = Draws.Load4(DrawId * 48);
    uint4             Buffers = Draws.Load4(DrawId * 48 + 16);
    uint4             Geometry = Draws.Load4(DrawId * 48 + 32);
    ByteAddressBuffer Indices = ResourceDescriptorHeap[NonUniformResourceIndex(Buffers.w)];
    uint              IndexPosition = (Geometry.y + VertexId) * Geometry.x;
    uint              Packed = Indices.Load(IndexPosition & ~3u);
    uint              Index = Geometry.x == 2 ? ((Packed >> ((IndexPosition & 2u) * 8u)) & 0xffffu) : Packed;
    return BuildVertex(uint(int(Index) + asint(Geometry.z)), Object.x, Object.y, Object.z, Buffers.x, Buffers.y, Buffers.z);
}

float3x3 CotangentFrame(float3 N, float3 P, float2 Uv)
{
    float3 Dp1 = ddx(P);
    float3 Dp2 = ddy(P);
    float2 Duv1 = ddx(Uv);
    float2 Duv2 = ddy(Uv);

    float3 Dp2perp = cross(Dp2, N);
    float3 Dp1perp = cross(N, Dp1);
    float3 T = Dp2perp * Duv1.x + Dp1perp * Duv2.x;
    float3 B = Dp2perp * Duv1.y + Dp1perp * Duv2.y;

    float Invmax = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-20));

    float W = (dot(cross(N, T), B) < 0.0) ? -1.0 : 1.0;

    T = T * W;

    return transpose(float3x3(T * Invmax, B * Invmax, N));
}

float3 PerturbNormal(float3 N, float3 V, float3 NormalSample, float2 Uv)
{
    float3   Map = normalize(2.0 * NormalSample - float3(1.0));
    float3x3 TBN = CotangentFrame(N, V, Uv);
    float3 Perturbed = mul(TBN, Map);
    return dot(Perturbed, Perturbed) > 1e-20 ? normalize(Perturbed) : N;
}

void RunAlphaTest(float Alpha, float AlphaThreshold, float2 Pixel)
{
    if (AlphaThreshold > 0.0)
    {
        // http://alex-charlton.com/posts/Dithering_on_the_GPU/
        // https://forums.khronos.org/showthread.php/5091-screen-door-transparency
        float4x4 ThresholdMatrix =
            float4x4(1.0 / 17.0, 9.0 / 17.0, 3.0 / 17.0, 11.0 / 17.0, 13.0 / 17.0, 5.0 / 17.0, 15.0 / 17.0, 7.0 / 17.0, 4.0 / 17.0,
                     12.0 / 17.0, 2.0 / 17.0, 10.0 / 17.0, 16.0 / 17.0, 8.0 / 17.0, 14.0 / 17.0, 6.0 / 17.0);

        Alpha = clamp(Alpha - 0.5 * ThresholdMatrix[int(fmod(Pixel.x, 4.0))][int(fmod(Pixel.y, 4.0))], 0.0, 1.0);

        if (Alpha < AlphaThreshold)
        {
            discard;
        }
    }
}

PSOutput PSMain(PSInput Input, bool FrontFace : SV_IsFrontFace)
{
    SamplerState      SurfaceSampler = SamplerDescriptorHeap[SamplerIndex];
    ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
    GpuMaterialData   Material = Materials.Load<GpuMaterialData>(Input.MaterialData.x * sizeof(GpuMaterialData));

    float4 BaseColorValue = Material.BaseColorFactor;
    if (Material.BaseColorTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> BaseColor = ResourceDescriptorHeap[NonUniformResourceIndex(Material.BaseColorTexture)];
        BaseColorValue *= BaseColor.Sample(SurfaceSampler, Input.Uv);
    }
    if (Material.AlphaMode == 1)
    {
        RunAlphaTest(BaseColorValue.a, Material.AlphaCutoff / max(32.0 * fwidth(float(Input.Uv.x)), 1.0), Input.Position.xy);
    }

    float3 N = SurfaceNormal(Input.Normal, Input.WorldPosition);
    if (Material.NormalTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> NormalMap = ResourceDescriptorHeap[NonUniformResourceIndex(Material.NormalTexture)];
        float3            NormalSample = NormalMap.Sample(SurfaceSampler, Input.Uv).xyz;
        // Bistro uses BC5 Normal maps: reconstruct Z before the reference RGB Normal mapping.
        float2 NormalXY = NormalSample.xy * 2.0 - 1.0;
        NormalSample.z = 0.5 + 0.5 * sqrt(saturate(1.0 - dot(NormalXY, NormalXY)));
        if (length(NormalSample) > 0.5)
        {
            N = PerturbNormal(N, Input.WorldPosition, NormalSample, Input.Uv);
        }
    }

    N = FrontFace ? N : -N;

    float Roughness = Material.RoughnessFactor;
    float Metallic = Material.MetallicFactor;
    if (Material.MetallicRoughnessTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> MetallicRoughness = ResourceDescriptorHeap[NonUniformResourceIndex(Material.MetallicRoughnessTexture)];
        float4            MetallicRoughnessValue = MetallicRoughness.Sample(SurfaceSampler, Input.Uv);
        Roughness *= MetallicRoughnessValue.g;
        Metallic *= MetallicRoughnessValue.b;
    }
    Roughness = saturate(Roughness);
    Metallic = saturate(Metallic);

    float3 Emissive = Material.EmissiveFactor;
    if (Material.EmissiveTexture != INVALID_TEXTURE)
    {
        Texture2D<float4> EmissiveTexture = ResourceDescriptorHeap[NonUniformResourceIndex(Material.EmissiveTexture)];
        Emissive *= EmissiveTexture.Sample(SurfaceSampler, Input.Uv).rgb;
    }

    float2 CurrentNdc = Input.CurrentClip.xy / Input.CurrentClip.w;
    float2 PreviousNdc = Input.PreviousClip.xy / Input.PreviousClip.w;
    float2 Motion = (CurrentNdc - PreviousNdc) * float2(0.5, -0.5);

    // Disney/UE default: Specular 0.5 maps to a dielectric F0 of 0.04.
    float2 PackedMaterial = PackMaterialProperties(Roughness, 0.5);

    PSOutput Output;
    Output.Emissive = float4(Emissive, 0.0);
    Output.NormalMaterial = float4(EncodeOctNormal(N), PackedMaterial.x, PackedMaterial.y);
    Output.BaseColorMetallic = float4(BaseColorValue.rgb, Metallic);
    Output.MotionMaterialId = float4(EncodeMotionVector(Motion), min(Input.MaterialData.y, 255u) / 255.0);
    return Output;
}
