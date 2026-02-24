## 2026-02-25 - Fix DX12 Backend Bugs for Deferred Rendering Pipeline

### 完成内容
- 修复了 8 个阻止 DX12 延迟渲染管线运行的关键 Bug：
  1. **内存释放不匹配**：`RenderTarget::~RenderTarget()` 中 `delete m_texture` 改为 `Memory::Free(m_texture)`，匹配 `Memory::Alloc` 分配方式
  2. **顶点步幅缺失**：geometry pass 中 5 个顶点属性的 `stride` 未设置（默认 0），导致 `BindVertexBuffers` 失败。设为 `sizeof(Vertex)` (52 bytes)
  3. **ExecuteIndirect 空指针**：`DrawIndirectIndexedInstanced` 传入 `nullptr` 作为 command signature。在 `RHIDX12::InitializeDX12Renderer` 中创建 `ID3D12CommandSignature`，存储在 `DX12RendererContext` 中
  4. **ClearBuffer 需要 pipeline**：`ClearBuffer`/`ClearTexture` 通过 `m_current_pipeline` 访问 descriptor heap allocator，但 resource upload pass 无绑定 pipeline。重构为支持直接通过 `DX12CommandList` 的 allocator 引用访问
  5. **描述符堆重置问题**：`ResetDescriptorHeaps()` 重置所有索引包括 RTV/DSV，导致持久 render target 的句柄失效。改为仅重置 SRV/UAV/CBV、Sampler 和 Staging 索引
  6. **死代码移除**：`FrameGraph::Execute()` 中的永远为 false 的内部 if 判断
  7. **双重 ImportResources**：`AddPass(RDGPass*)` 内部调用了 `ImportResources`，但 app 也显式调用了。移除了 `AddPass` 内部的调用
  8. **SwapChain 纹理元数据**：back buffer texture 使用 `DUMMY_COLOR` 格式创建，元数据（宽高、格式、array_layer、mip_map_level、state）未初始化。赋值 back buffer 后设置正确元数据

### 关键决策
- 为支持 ClearBuffer 无 pipeline 场景，给 `DX12CommandList` 添加了 `DX12DescriptorHeapAllocator*` 成员，通过 `DX12CommandContext` 传递
- Command signature 存储在 `DX12RendererContext` 中（全局共享），而非每个 command list
- `Texture` 成员变量移除了 `const` 限定（`m_width`, `m_height`, `m_format`, `m_type`, `m_array_layer`, `m_byte_per_pixel`），以支持 swap chain back buffer 的后初始化
- RTV/DSV 索引保持跨帧稳定，仅 shader-visible heap 索引每帧重置

### 踩坑记录
- `Texture` 的多个成员被声明为 `const`，阻止了 swap chain back buffer 的元数据设置。移除 `const` 是最小侵入性的修改
- `ClearUnorderedAccessViewUint` 需要同时在 shader-visible 和 non-shader-visible (staging) 堆上创建 UAV 描述符

### 待办事项
- 运行 deferred sample 验证渲染结果
- 检查 D3D12 debug layer 是否有错误/警告
- 验证多帧运行无描述符堆溢出
- 考虑将 sampler 也改为持久分配（目前每帧重置）

## 2026-02-25 - Fix DX12 Root Signature Not Matching Vertex Shader SRV

### 完成内容
- 修复了 `D3D12 ERROR: Root Signature doesn't match Vertex Shader: Shader SRV descriptor range (BaseShaderRegister=0, NumDescriptors=1, RegisterSpace=0) is not fully bound in root signature` 错误
- 根因：`ReflectShaderDXIL()` 使用 `IDxcContainerReflection` + `CreateBlob(CP_UTF8)` 包装二进制 DXIL bytecode 做反射，对 SM 6.6 shader 不可靠，反射静默失败导致 vertex shader 的 SRV (t0, space0) 未被识别
- 改用 DXC 编译器直接输出的 `DXC_OUT_REFLECTION` + `IDxcUtils::CreateReflection` 进行可靠反射

### 关键决策
- Reflection 数据在编译时通过 `IDxcResult::GetOutput(DXC_OUT_REFLECTION)` 提取，作为 `IDxcBlob*` 传递给 `DX12Shader`，由 shader 对象持有并在析构时释放
- 完全移除了 `IDxcContainerReflection` 全局单例（`g_dxc_reflection`），简化了代码路径

### 踩坑记录
- `IDxcContainerReflection` + `CreateBlob(CP_UTF8)` 对 SM 6.6 DXIL bytecode 反射不可靠，会静默丢失 shader 资源绑定信息。正确做法是在编译时直接从 `IDxcResult` 提取 `DXC_OUT_REFLECTION` 输出
- DXC 编译器已在注释代码中提示了正确方案（`dx12_shader_compiler.cpp` 358-365 行的注释）

### 修改文件
1. `dx12_shader_compiler.h` — `CompileHLSL`/`CompileHLSLWithDXC` 添加 `IDxcBlob** out_reflection` 参数；移除 `GetDXCReflection()` 声明
2. `dx12_shader_compiler.cpp` — 编译后提取 `DXC_OUT_REFLECTION`；移除 `g_dxc_reflection` 全局变量、初始化、清理和 getter
3. `dx12_shader.h` — 构造函数添加 `IDxcBlob* reflection_blob` 参数；添加 `m_reflection_blob` 成员
4. `dx12_shader.cpp` — 用 `IDxcUtils::CreateReflection(&DxcBuffer)` 替换 `IDxcContainerReflection` 方式
5. `rhi_dx12.cpp` — `CreateShader` 中获取 reflection blob 并传给 `DX12Shader`

### 待办事项
- 运行 deferred sample 验证 root signature 不再报错
- 确认所有 shader 资源（instance_parameter, CameraParamsUb_cb 等）正确绑定
