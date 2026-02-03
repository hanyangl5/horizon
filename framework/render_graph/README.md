# FrameGraph 使用指南

FrameGraph 是一个自动管理资源生命周期和 barrier 插入的渲染图系统。

## 基本概念

1. **资源句柄 (Handles)**: `TextureHandle`, `BufferHandle`, `RenderTargetHandle` - 用于在 FrameGraph 中引用资源
2. **Pass**: 渲染或计算任务，通过回调函数执行
3. **自动 Barrier**: FrameGraph 会根据资源的读写状态自动插入 barrier

## 基本使用流程

```cpp
// 1. 创建 FrameGraph
frame_graph = std::make_unique<FrameGraph>(rhi);

// 2. 在每帧开始时重置
frame_graph->Reset();

// 3. 添加 Pass
frame_graph->AddPass("Geometry Pass", [&](CommandList *cl, FrameGraphBuilder &builder) {
    // 导入或创建资源
    auto gbuffer0 = builder.ImportRenderTarget("gbuffer0", deferred->gbuffer0);
    
    // 声明资源使用
    builder.UseRenderTarget(gbuffer0);
    
    // 执行渲染命令
    RenderPassBeginInfo begin_info{...};
    cl->BeginRenderPass(begin_info);
    // ... 绘制命令
    cl->EndRenderPass();
});

// 4. 编译 FrameGraph（创建 transient 资源）
frame_graph->Compile();

// 5. 执行所有 Pass（自动插入 barrier）
frame_graph->Execute();
```

## 资源管理

### 导入外部资源
```cpp
auto texture = builder.ImportTexture("my_texture", existing_texture);
builder.ReadTexture(texture, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
```

### 创建临时资源
```cpp
auto temp_buffer = builder.CreateBuffer("temp_buffer", BufferCreateInfo{...});
builder.WriteBuffer(temp_buffer, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
```

## 自动 Barrier

FrameGraph 会自动跟踪资源状态，并在需要时插入 barrier：

- 当 Pass 读取资源时，会从上一个 Pass 的状态转换到读取状态
- 当 Pass 写入资源时，会从上一个 Pass 的状态转换到写入状态
- 如果资源是第一次使用，会从 `UNDEFINED` 状态转换

## 示例：Geometry Pass

```cpp
frame_graph->AddPass("Geometry Pass", [&](CommandList *cl, FrameGraphBuilder &builder) {
    // 导入 G-buffer
    auto gbuffer0 = builder.ImportRenderTarget("gbuffer0", deferred->gbuffer0);
    auto gbuffer1 = builder.ImportRenderTarget("gbuffer1", deferred->gbuffer1);
    auto depth = builder.ImportRenderTarget("depth", deferred->depth);
    
    // 使用 render target（会自动处理 barrier）
    builder.UseRenderTarget(gbuffer0);
    builder.UseRenderTarget(gbuffer1);
    builder.UseRenderTarget(depth);
    
    // 设置 render pass
    RenderPassBeginInfo begin_info{...};
    begin_info.render_targets[0].data = builder.GetRenderTarget(gbuffer0);
    begin_info.render_targets[1].data = builder.GetRenderTarget(gbuffer1);
    begin_info.depth_stencil.data = builder.GetRenderTarget(depth);
    
    cl->BeginRenderPass(begin_info);
    cl->BindPipeline(deferred->geometry_pass);
    // ... 绘制
    cl->EndRenderPass();
});
```

## 注意事项

1. **资源命名**: 使用相同的名称会复用同一个资源句柄
2. **Transient 资源**: 默认创建的资源是 transient 的，会在 Reset() 时销毁
3. **导入的资源**: 导入的资源不会被 FrameGraph 管理生命周期
4. **Pass 顺序**: 目前按添加顺序执行，未来会支持拓扑排序优化
