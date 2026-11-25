#include "../../Examples/Renderer/Shaders/Common.slang"
#include "../../Examples/Renderer/Shaders/DeferredLightCommon.slang"

cbuffer RootConstant0
{
    uint DataIndex;
    uint FrameIndex;
    uint OutputIndex;
};

[numthreads(8, 8, 1)]
void CompareLighting(uint3 ID : SV_DispatchThreadID)
{
    ByteAddressBuffer Data = ResourceDescriptorHeap[DataIndex];
    const ClusterGrid Grid = Data.Load<ClusterGrid>(0);
    if (ID.x >= Grid.Width || ID.y >= Grid.Height)
        return;
    // Two fragments of the same pixel may occupy different bins (opaque and OIT).
    static const float Depths[] = {0.11, 1, 3, 7, 15.99};
    const float Z = Depths[ID.z];
    const float2 Ndc = (float2(ID.xy) + 0.5) / float2(Grid.Width, Grid.Height) * float2(2,-2) + float2(-1,1);
    const float3 P = float3((Ndc + float2(Grid.ClipX.z, Grid.ClipY.z)) * Z / float2(Grid.ClipX.x, Grid.ClipY.y), -Z);
    const FGBufferData Surface = MakeDefaultLitMaterial(float3(0,0,1), float3(0.8,0.3,0.1), 0.2, 0.5, 0.5);
    StructuredBuffer<LightingFrameData> Frame = ResourceDescriptorHeap[FrameIndex];
    const float3 All = AccumulateAdditionalLights(Frame[0], Surface, P, normalize(-P));
    const float3 Culled = AccumulateScreenLights(DataIndex, ID.xy, Frame[0], Surface, P, normalize(-P));
    RWStructuredBuffer<float4> Result = ResourceDescriptorHeap[OutputIndex];
    const float3 Error = abs(All - Culled) / (1e-5 + abs(All) * 2e-5);
    Result[(ID.z * Grid.Height + ID.y) * Grid.Width + ID.x] = float4(Error, max(All.x,max(All.y, All.z)));
}

[numthreads(1, 1, 1)]
void CompareSourceRadii()
{
    const FGBufferData Surface = MakeDefaultLitMaterial(float3(0, 0, 1), 0.8.xxx, 0, 0.4, 0.5);
    const float3 V = float3(0, 0, 1);
    const float3 L = normalize(float3(1, 0, 1));
    const FDirectLighting Point = DefaultLitBxDF(Surface, V, L);
    const FDirectLighting Sphere = DefaultLitBxDF(Surface, V, L, 0.2);
    RWStructuredBuffer<float4> Result = ResourceDescriptorHeap[OutputIndex];
    Result[0] = float4(Point.Diffuse.x, Sphere.Diffuse.x, Point.Specular.x, Sphere.Specular.x);
}
