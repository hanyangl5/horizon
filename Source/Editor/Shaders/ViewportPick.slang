void ClipSelectionSurface(PSInput Input)
{
    ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
    GpuMaterialData Material = Materials.Load<GpuMaterialData>(Input.MaterialData.x * sizeof(GpuMaterialData));
    if (Material.AlphaMode == 1)
    {
        SamplerState SurfaceSampler = SamplerDescriptorHeap[SamplerIndex];
        float Alpha = Material.BaseColorFactor.a;
        if (Material.BaseColorTexture != INVALID_TEXTURE)
        {
            Texture2D<float4> BaseColor = ResourceDescriptorHeap[NonUniformResourceIndex(Material.BaseColorTexture)];
            Alpha *= BaseColor.Sample(SurfaceSampler, Input.Uv).a;
        }
        RunAlphaTest(Alpha, Material.AlphaCutoff / max(32.0 * fwidth(Input.Uv.x), 1.0), Input.Position.xy);
    }
}

PSInput MaskVS(uint VertexId : SV_VertexID)
{
    PSInput Output = VSIndirect(VertexId);
    ByteAddressBuffer Draws = ResourceDescriptorHeap[DrawsIndex];
    ByteAddressBuffer Instances = ResourceDescriptorHeap[InstancesIndex];
    const uint Instance = Draws.Load(InstanceIndex * 48);
    Output.MaterialData.y = uint(all(Instances.Load2(Instance * 192 + 8) == uint2(MaterialIndex, MaterialId)));
    return Output;
}

float MaskPS(PSInput Input) : SV_Target0
{
    ClipSelectionSurface(Input);
    return float(Input.MaterialData.y);
}

uint2 PickPS(PSInput Input) : SV_Target0
{
    ClipSelectionSurface(Input);
    ByteAddressBuffer Draws = ResourceDescriptorHeap[DrawsIndex];
    ByteAddressBuffer Instances = ResourceDescriptorHeap[InstancesIndex];
    uint Instance = Draws.Load(InstanceIndex * 48);
    return Instances.Load2(Instance * 192);
}
