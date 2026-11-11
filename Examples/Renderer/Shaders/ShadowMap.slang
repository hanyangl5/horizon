// Adapted from Cookbook shadow.frag.slang; see ../Cookbook-LICENSE.txt.
#include "Geometry.slang"
void PSShadow(PSInput input)
{
    ByteAddressBuffer materials = ResourceDescriptorHeap[materialsIndex];
    GpuMaterialData   material = materials.Load<GpuMaterialData>(input.materialData.x * sizeof(GpuMaterialData));
    if (material.alphaMode == 1)
    {
        float alpha = material.baseColorFactor.a;
        if (material.baseColorTexture != INVALID_TEXTURE)
        {
            Texture2D<float4> base = ResourceDescriptorHeap[NonUniformResourceIndex(material.baseColorTexture)];
            SamplerState      sampler = SamplerDescriptorHeap[samplerIndex];
            alpha *= base.Sample(sampler, input.uv).a;
        }
        clip(alpha - material.alphaCutoff);
    }
}
