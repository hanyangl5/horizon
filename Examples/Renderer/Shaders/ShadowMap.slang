// Adapted from Cookbook shadow.frag.slang; see ../Cookbook-LICENSE.txt.
#include "Geometry.slang"
void PSShadow(PSInput Input)
{
    ByteAddressBuffer Materials = ResourceDescriptorHeap[MaterialsIndex];
    GpuMaterialData   Material = Materials.Load<GpuMaterialData>(Input.MaterialData.x * sizeof(GpuMaterialData));
    if (Material.AlphaMode == 1)
    {
        float Alpha = Material.BaseColorFactor.a;
        if (Material.BaseColorTexture != INVALID_TEXTURE)
        {
            Texture2D<float4> Base = ResourceDescriptorHeap[NonUniformResourceIndex(Material.BaseColorTexture)];
            SamplerState      Sampler = SamplerDescriptorHeap[SamplerIndex];
            Alpha *= Base.Sample(Sampler, Input.Uv).a;
        }
        clip(Alpha - Material.AlphaCutoff);
    }
}
