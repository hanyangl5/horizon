# GPUScene

实现见 [GpuScene.h](../Source/Runtime/Scene/Public/Scene/GpuScene.h)。渲染侧只有一份 CPU 场景（RenderScene）和一份常驻 GPU 场景（GpuScene），轮换的只有上传用的 staging。

## 主流做法

| 引擎 | 渲染侧场景 | GPU 数据更新 | 资源释放 |
| --- | --- | --- | --- |
| Unreal | `FScene`，渲染线程持有，SceneProxy 镜像组件 | `FGPUScene` 常驻 buffer，dirty 列表，compute scatter | RHI 延迟删除 |
| Unity Entities Graphics | ECS chunk 数据 | 常驻 GraphicsBuffer，`SparseUploader` scatter | 按帧延迟释放 |
| Bevy | 独立 RenderWorld，每帧 extract | 渲染侧 buffer | wgpu 资源引用计数 |

共同点：渲染侧只有一份场景，GPU 上只有一份常驻数据，多份轮换的只有上传用的 staging。

能这样做的原因是队列顺序：第 N+1 帧的更新排在第 N 帧的读取之后执行，所以单份 buffer 不会被提前改写。

## 设计

```text
World --extract--> RenderScene --dirty--> staging --copy--> GpuScene buffers --> passes
                   (CPU，唯一一份)        (CPU_TO_GPU)       (GPU_ONLY，唯一一份)
```

- `RenderScene` 对应 Unreal 的 `FScene`：它是渲染侧唯一的 CPU 副本，只有 `Update()` 会修改它。Renderer 录制命令时直接读取，不再复制到每帧数组。
- `GpuScene` 对应 `FGPUScene`：持有常驻 buffer 和每帧 staging。

```cpp
class GpuScene
{
public:
    GpuScene(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSceneDesc& desc = {});
    // Extracts World and fills staging. Returns false when every frame is still in GPU use; never waits.
    bool BeginFrame();
    void Upload(CommandList& commands);
    // Clears uploaded dirty state and advances render history.
    void EndFrame(SubmitHandle submission);
    // Keeps the changes pending for the next frame.
    void CancelFrame();
    const GpuSceneBuffers& GetBuffers() const;
    const GpuAssetTables&  GetAssetTables() const;
};
```

每帧流程：

```text
if (!gpuScene.BeginFrame()) return;
commands = context.AcquireCommandList();
gpuScene.Upload(commands);
... passes 读取 RenderScene 和 GpuScene buffers ...
gpuScene.EndFrame(context.Submit(commands, &backbuffer));
```

不提交时调用 `CancelFrame`，dirty 不清除、历史不推进，下一帧自动补上。`BeginFrame` 与 `EndFrame` 之间不能调用 `RenderScene::Update` 或 `MarkDirty`，用 `ASSERT` 检查。World 在此期间可以继续修改，下一次提取才可见。

### Dirty 与上传

- 每张表为每个 索引 记一个待上传标记和待上传列表，提取时标记，`EndFrame` 清除。标记跨多次 `Update` 累积。
- `BeginFrame` 把待上传 索引 合并成 range 写入 staging；active list 变化时整表上传，`instanceIndices` 按 range 上传。
- 每个帧槽有一块持久映射的 `CPU_TO_GPU` staging，按 `SubmitHandle` 回收（默认三份）；空间不足时换更大的一块，旧块延迟释放。
- `CommandList::CopyBufferRegions` 一次拷贝多个 range，每张表只做一对 barrier。
- 常驻 buffer 用 `GPU_ONLY` 和 raw SRV。range 过多时再切换到 compute scatter，届时再加 UAV；阈值由 profile 决定，不影响 buffer layout。

### 扩容与资源释放

`RenderContext::DeferRelease` 把 buffer 交给延迟释放队列：正在录制的命令列表和更早的提交都完成后才销毁，`WaitIdle` 时回收。`GetResourceUseId`/`IsResourceUseComplete` 用同一判断供 Asset Manager 使用。

扩容时创建新 buffer 并整表重新上传，旧 buffer 交给 `DeferRelease`。新 buffer 在提交前被取消时，下一帧仍整表上传。GpuScene 析构不等待 GPU，所有 buffer 交给 `DeferRelease`。Bindless 索引随之变化，Renderer 每帧都从 `GetBuffers()` 取索引，因此不受影响。

### 资产表

- Mesh、Submesh、Material、Texture 表也进入 `GpuScene`，只保留一份，和 RenderScene 的表在同一次 `Upload` 中更新。两边的配对由队列顺序保证。
- `AssetManager::GetGpuTables` 按 Ready 集合的版本缓存编码，集合变化时整表上传；之后再改为按 索引 更新。
- Asset Manager 构造时传入 RenderContext；引用归零时记下 `GetResourceUseId()`，对应工作完成后才销毁 payload。Hot reload 的旧版本走同一路径。

### 运动历史

`RenderInstance` 继续同时保存 current 和 previous。`EndFrame` 只为本帧有变化的 instance 记录已提交的 current；下一次提取据此计算 previous，有变化时标记 dirty 并上传。静止实体没有开销。`MarkDirty` 在下一次提取前阻止记录旧值。

### 渲染线程

现在不实现。以后 RenderScene 和 GpuScene 归渲染线程所有，World → RenderScene 的提取就是两个线程的同步点，即 Bevy 的 extract。渲染线程可以录制第 N 帧，同时主线程更新第 N+1 帧的 World，不需要增加 CPU 副本。这与 `Multithreading.md` 的 finalized scene snapshot 一致：冻结的是 RenderScene，frame resources 三缓冲的是 staging。

## 约束与代价

- 每帧开头多一段 copy 和 barrier，只有 dirty 的 buffer 才需要。
- 单份 buffer 依赖单队列顺序。以后接入 async compute 或 copy queue 时，必须显式同步跨队列的读写。
- 不再有可以跨帧保留的场景快照。需要固定某一帧数据的功能（如截帧分析）要自己复制。
- GPU 记录仍直接 memcpy CPU 结构体，包括 `Guid` 这类着色器不读的字段。拆分 GPU 专用 layout 不在本方案范围内。
- 部分表（如 primitives、meshBatches）当前 Renderer 不读取，为 GPU-driven 渲染预留。

## 验证

- GPU 读回：首帧整表上传、未改动的表跨帧保留、多次 update 累积、取消帧、删除与 索引 复用后，常驻 buffer 与 RenderScene 一致；资产离开 Ready 集合后 GPU 表中的记录失效。
- 延迟销毁：命令列表仍在录制时释放的纹理、Mesh 和材质，在提交完成前保持存活。去掉该判断或历史提交时，对应测试失败。
- 运动历史：取消帧不推进历史，teleport、相机 cut 和 索引 复用使用 previous=current。
- D3D12 GPU validation 下测试和 Bistro 均无错误。
- 增量提取：`WorldChangeLog` 记录组件编辑、创建删除和世界变换变化的实体，`RenderScene` 与资产绑定每帧只处理这些实体、提交后历史需要推进的实体和仍在等待资产的绑定。资产状态变化、层级重建或日志溢出时退回全量扫描。
