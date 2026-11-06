# GPUScene

状态：方案，尚未实现。目标是用主流引擎的常驻 GPU 场景取代 `RenderFramePool`。World、RenderScene 的提取规则和 `.world.json` 不变。

## 现状的问题

`RenderFramePool` 轮换三份 backing，每份都有完整的 CPU 数组和 16 个 GPU buffer：

- 同一份场景存 6 份（3 份 CPU、3 份 GPU）。Bistro 只有几百 KB，影响很小；100 万 instance 时，仅 `RenderInstance`（192 字节）就超过 1 GB。
- 场景 buffer 是 `CPU_TO_GPU`，落在 D3D12 UPLOAD 堆，着色器直接从这里读。独显上这通常是走 PCIe 的系统内存，不适合 GPU-driven 反复读取。
- 每份 backing 落后的版本不同，所以需要 revision、固定容量 journal 和溢出后的完整同步。
- `AssetSnapshot` 固定全部 Ready payload，资产集合一变就重建，并为每份 backing 上传完整的表。
- `commitSnapshot` 每帧遍历全部实体，并需要 `historyEpoch` 防止旧帧覆盖新的重置。

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
World --extract--> RenderScene --dirty--> staging ring --copy--> GpuScene buffers --> passes
                   (CPU，唯一一份)        (CPU_TO_GPU)            (GPU_ONLY，唯一一份)
```

- `RenderScene` 对应 Unreal 的 `FScene`：它是渲染侧唯一的 CPU 副本，只有 `update()` 会修改它。Renderer 录制命令时直接读取，不再复制到每帧数组。
- `GpuScene` 对应 `FGPUScene`：持有常驻 buffer 和 staging ring，取代 `RenderFramePool` 和 `RenderFrame`。

```cpp
class GpuScene
{
public:
    GpuScene(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSceneDesc& desc = {});
    // Extracts World and fills staging. Returns false when too many frames are in flight; never waits.
    bool beginFrame();
    void recordUpload(CommandList& commands);
    // Clears uploaded dirty state and advances render history.
    void endFrame(SubmitHandle submission);
    const GpuSceneBuffers& getBuffers() const;
};
```

每帧流程：

```text
if (!gpuScene.beginFrame()) return;
commands = context.acquireCommandList();
gpuScene.recordUpload(commands);
... passes 读取 RenderScene 和 GpuScene buffers ...
gpuScene.endFrame(context.submit(commands, &backbuffer));
```

没有调用 `endFrame` 的帧不清除 dirty，也不推进历史，下一帧自动补上，与现在丢弃快照的语义一致。`beginFrame` 与 `endFrame` 之间不能调用 `RenderScene::update` 或 `resetHistory`，用 `ASSERT` 检查。World 在此期间可以继续修改，下一次提取才可见。

### Dirty 与上传

- 每张表用 1 bit/slot 的位图记录 dirty，提取时置位，`endFrame` 清零。位图容量固定，跨多次 `update` 自然累积，不再需要 journal。
- `beginFrame` 把位图合并成 range，并把数据写入 staging ring。active list 和 `instanceIndices` 变化时整表上传。
- staging ring 是一块持久映射的 `CPU_TO_GPU` buffer，按 `SubmitHandle` 回收。空间不足时换更大的一块，旧块延迟释放。
- 第一版用批量 copy。`CommandList` 需要新增一次 barrier 处理多个 range 的接口，因为现有 `copyBuffer` 每次调用都做两组 barrier，`updateBuffer` 每次调用都新建 buffer，都不能用于每帧路径。
- 常驻 buffer 用 `GPU_ONLY`，从一开始同时创建 SRV 和 UAV。range 过多时再切换到 compute scatter；阈值由 profile 决定，不影响 buffer layout。

### 扩容与资源释放

`RenderContext` 新增延迟释放队列：资源进入队列后，在已经录制或提交、可能引用它的工作完成后才销毁；`waitIdle` 时清空。当前 `GPUBuffer::destroy` 立即释放，现有代码靠 backing 的 fence 保证安全，单份 buffer 后这个队列是必需的。

扩容时创建新 buffer，把旧内容 copy 过去，旧 buffer 进入延迟释放队列。Bindless 索引随之变化，Renderer 每帧都从 `getBuffers()` 取索引，因此不受影响。

### 资产表

- Mesh、Submesh、Material、Texture 表也进入 `GpuScene`，只保留一份，和 RenderScene 的表在同一次 `recordUpload` 中更新。两边的配对由队列顺序保证，不再需要配对快照。
- 第一版沿用现有编码，资产集合变化时整表上传；之后再改为按 slot 更新。
- `AssetSnapshot` 删除。Payload 从表中移除后，记录当帧的 `SubmitHandle`，fence 完成后销毁。Hot reload 的旧版本走同一路径。

### 运动历史

`RenderInstance` 继续同时保存 current 和 previous。`endFrame` 只处理上一帧有变化的实体：把 previous 更新为 current，并标记 dirty，下一帧上传。静止实体没有开销。因为提交和重置都在主线程的帧边界，`historyEpoch` 可以删除。

### 渲染线程

现在不实现。以后 RenderScene 和 GpuScene 归渲染线程所有，World → RenderScene 的提取就是两个线程的同步点，即 Bevy 的 extract。渲染线程可以录制第 N 帧，同时主线程更新第 N+1 帧的 World，不需要增加 CPU 副本。这与 `Multithreading.md` 的 finalized scene snapshot 一致：冻结的是 RenderScene，frame resources 三缓冲的是 staging。

## 删除的内容

`RenderFramePool`、`RenderFrame`、每帧 CPU 数组、`RenderSceneSync` 和 `getChangesSince`、journal、scene/asset revision、`commitSnapshot` 的快照参数、`historyEpoch`、`AssetSnapshot`。

## 约束与代价

- 每帧开头多一段 copy 和 barrier，只有 dirty 的 buffer 才需要。
- 单份 buffer 依赖单队列顺序。以后接入 async compute 或 copy queue 时，必须显式同步跨队列的读写。
- 不再有可以跨帧保留的场景快照。需要固定某一帧数据的功能（如截帧分析）要自己复制。
- GPU 记录仍直接 memcpy CPU 结构体，包括 `Guid` 这类着色器不读的字段。拆分 GPU 专用 layout 不在本方案范围内。
- Renderer 目前只读取 instances、materials、textures、lights 和 activeLights，其余的表是为 GPU-driven 预留的，继续保留。

## 实施步骤

每一步都可以单独测试和合入：

1. `RenderContext`：延迟释放队列、批量 copy 接口。
2. `GpuScene`：常驻 buffer、staging ring、dirty 位图；先让 Renderer 和回归测试切换过去，`AssetSnapshot` 暂时保留。
3. Renderer 自己管理每帧常量 buffer，不再依赖 `getBackingIndex()`。
4. 资产表并入 `GpuScene`，用按 `SubmitHandle` 退休取代 `AssetSnapshot`。
5. 删除 `RenderFramePool` 及上面列出的内容，更新 `WorldFoundation.md` 和 `WorldTutorial.md`。
6. Profile 后决定是否加入 compute scatter 和按 slot 更新资产表。

## 验证

- GPU 读回：连续多帧修改、删除、复用 slot 后，常驻 buffer 与 RenderScene 一致。
- 丢帧：`beginFrame` 后不提交，下一帧数据和历史都正确。
- 并发安全：GPU 仍在读时释放资产、扩容 buffer，D3D12 GPU validation 无错误。
- 运动矢量：现有 previous 回归继续通过，静止实体只上传一次 previous=current。
- 预热后的稳定帧和普通变换更新不分配内存；关闭时延迟释放队列为空。
- 用 10 万 Entity stress case 对比替换前后的 CPU 帧时间和上传字节数。
