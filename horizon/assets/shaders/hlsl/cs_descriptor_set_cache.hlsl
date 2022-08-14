
// StructuredBuffer<T> RWBufferResources[1024];

// ConstantBuffer<> UniformBufferResource
// TextureResources
// RWTextrueResouarce
// StructuredBuffer<StructA>
struct T {
  float e;
};
// set 0
[[vk::binding(0, 0)]] ConstantBuffer<T> ub1;
[[vk::binding(1, 0)]] ConstantBuffer<T> ub2;

// set 1
[[vk::binding(0, 1)]] RWStructuredBuffer<T> sb1;
[[vk::binding(1, 1)]] RWStructuredBuffer<T> sb2;

[[vk::binding(0, 2)]] ConstantBuffer<T> ub3;
[[vk::binding(1, 2)]] ConstantBuffer<T> ub4;

// set 1
[[vk::binding(0, 3)]] RWStructuredBuffer<T> sb3;
[[vk::binding(1, 3)]] RWStructuredBuffer<T> sb4;

[numthreads(1, 1, 1)] void cs_main() {
  sb1[0].e = ub1.e;
  sb2[0].e = ub2.e;
  sb3[0].e = ub3.e;
  sb4[0].e = ub4.e;
}