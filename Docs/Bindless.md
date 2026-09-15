# Bindless rendering

Horizon 的 D3D12 后端只使用 bindless 资源绑定，`IGraphics.h` 中的 `BINDLESSRENDERING` 表示该能力。设备需要支持 Shader Model 6.6 及 Resource Binding Tier 3；实际 shader target 可以要求更高版本。

Buffer、Texture 和 Sampler 创建时分配 shader-visible descriptor，索引在资源生命周期内保持稳定。通过 `getSrvIndex()`、`getUavIndex()`、`getCbvIndex()` 和 Sampler 的 `getIndex()` 获取对应索引，再用 root constants 或 GPU Buffer 传给 shader。Texture 的 UAV 索引可以指定 mip。

```hlsl
cbuffer RootConstant0 : register(b0) { uint textureIndex; uint samplerIndex; };

float4 PSMain(float2 uv : TEXCOORD0) : SV_Target0
{
    Texture2D<float4> texture = ResourceDescriptorHeap[textureIndex];
    SamplerState surface = SamplerDescriptorHeap[samplerIndex];
    return texture.Sample(surface, uv);
}
```

RootSignature 只接受 root constants；常量块名称需包含 `RootConstant` 或 `PushConstant`。普通 CBV/SRV/UAV 和 Sampler 使用 heap indexing，线程间变化的资源索引需使用 `NonUniformResourceIndex()`。

资源索引不会延长资源生命周期。销毁资源前必须等待引用它的 GPU 工作完成，释放后的索引可以立即复用。绑定索引也不会自动执行 barrier：`Dependencies.buffers` 表示只读 Buffer，`storageBuffers` 表示 UAV 写入；Texture 分别使用 `sampledTextures` 和 `storageTextures`。

UI、字体、虚拟摇杆和骨骼调试使用同一绑定方式。`RuntimeShaders` 构建目标编译它们的 HLSL，并输出至构建目录各配置下的 `CompiledShaders/DIRECT3D12`；Application 默认从可执行文件旁加载这些二进制 shader。
