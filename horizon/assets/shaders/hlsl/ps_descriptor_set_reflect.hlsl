
// StructuredBuffer<T> RWBufferResources[1024];

// ConstantBuffer<> UniformBufferResource
// TextureResources
// RWTextrueResouarce
// StructuredBuffer<StructA>
struct T {
  float4 e;
};

//[[vk::binding(0, 0)]] Buffer<float4> UniformBufferResource[1024];
//[[vk::binding(0, 1)]] RWBuffer<float4> RWBuffersResources[];
[[vk::binding(0, 0)]] ConstantBuffer<T> UniformBufferResource[1024];
//[[vk::binding(0, 1)]] StructuredBuffer<float4> RWBufferResources[];
[[vk::binding(0, 2)]] Texture2D<float4> TextureResources[];
[[vk::binding(0, 3)]] RWTexture2D<float4> RWTextrueResouarce[];
[numthreads(1, 1, 1)] void cs_main() {
  // StructuredBuffer<T> buffer = ResourceDescriptorHeap[0]; // require update
  // of dxc and vulkan
  uint2 xy = uint2(0, 0);
  RWTextrueResouarce[0][xy] = UniformBufferResource[0].e;
  // RWBuffersResources[0][0] += UniformBufferResource[0].e;
  RWTextrueResouarce[0][xy] += TextureResources[0][xy];
  // RWBufferResources[0] = float4(0.0, 0.0, 0.0, 0.0);
}