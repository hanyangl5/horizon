# GPUScene

状态：已实现，见 [GpuScene.h](../Source/Runtime/Scene/Public/Scene/GpuScene.h)。它用主流引擎的常驻 GPU 场景取代了早期的 `RenderFramePool`；World、RenderScene 的提取规则和 `.world.json` 不变。

## 原设计的问题

早期的 `RenderFramePool` 轮换三份 backing，每份都有完整的 CPU 数组和 16 个 GPU buffer：

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
World --extract--> RenderScene --dirty--> staging --copy--> GpuScene buffers --> passes
                   (CPU，唯一一份)        (CPU_TO_GPU)       (GPU_ONLY，唯一一份)
```

- `RenderScene` 对应 Unreal 的 `FScene`：它是渲染侧唯一的 CPU 副本，只有 `update()` 会修改它。Renderer 录制命令时直接读取，不再复制到每帧数组。
- `GpuScene` 对应 `FGPUScene`：持有常驻 buffer 和每帧 staging，取代 `RenderFramePool` 和 `RenderFrame`。

```cpp
class GpuScene
{
public:
    GpuScene(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSceneDesc& desc = {});
    // Extracts World and fills staging. Returns false when every frame is still in GPU use; never waits.
    bool beginFrame();
    void upload(CommandList& commands);
    // Clears uploaded dirty state and advances render history.
    void endFrame(SubmitHandle submission);
    // Keeps the changes pending for the next frame.
    void cancelFrame();
    const GpuSceneBuffers& getBuffers() const;
    const GpuAssetTables&  getAssetTables() const;
};
```

每帧流程：

```text
if (!gpuScene.beginFrame()) return;
commands = context.acquireCommandList();
gpuScene.upload(commands);
... passes 读取 RenderScene 和 GpuScene buffers ...
gpuScene.endFrame(context.submit(commands, &backbuffer));
```

不提交时调用 `cancelFrame`，dirty 不清除、历史不推进，下一帧自动补上。`beginFrame` 与 `endFrame` 之间不能调用 `RenderScene::update` 或 `markDirty`，用 `ASSERT` 检查。World 在此期间可以继续修改，下一次提取才可见。

### Dirty 与上传

- 每张表为每个 索引 记一个待上传标记和待上传列表，提取时标记，`endFrame` 清除。标记跨多次 `update` 累积，不再需要 journal。
- `beginFrame` 把待上传 索引 合并成 range 写入 staging；active list 变化时整表上传，`instanceIndices` 按 range 上传。
- 每个帧槽有一块持久映射的 `CPU_TO_GPU` staging，按 `SubmitHandle` 回收（默认三份）；空间不足时换更大的一块，旧块延迟释放。
- `CommandList::copyBufferRegions` 一次拷贝多个 range，每张表只做一对 barrier。
- 常驻 buffer 用 `GPU_ONLY` 和 raw SRV。range 过多时再切换到 compute scatter，届时再加 UAV；阈值由 profile 决定，不影响 buffer layout。

### 扩容与资源释放

`RenderContext::deferRelease` 把 buffer 交给延迟释放队列：正在录制的命令列表和更早的提交都完成后才销毁，`waitIdle` 时回收。`getResourceUseId`/`isResourceUseComplete` 用同一判断供 Asset Manager 使用。

扩容时创建新 buffer 并整表重新上传，旧 buffer 交给 `deferRelease`。新 buffer 在提交前被取消时，下一帧仍整表上传。GpuScene 析构不等待 GPU，所有 buffer 交给 `deferRelease`。Bindless 索引随之变化，Renderer 每帧都从 `getBuffers()` 取索引，因此不受影响。

### 资产表

- Mesh、Submesh、Material、Texture 表也进入 `GpuScene`，只保留一份，和 RenderScene 的表在同一次 `upload` 中更新。两边的配对由队列顺序保证，不再需要配对快照。
- `AssetManager::getGpuTables` 按 Ready 集合的版本缓存编码，集合变化时整表上传；之后再改为按 索引 更新。
- `AssetSnapshot` 已删除。Asset Manager 构造时传入 RenderContext；引用归零时记下 `getResourceUseId()`，对应工作完成后才销毁 payload。Hot reload 的旧版本走同一路径。

### 运动历史

`RenderInstance` 继续同时保存 current 和 previous。`endFrame` 只为本帧有变化的 instance 记录已提交的 current；下一次提取据此计算 previous，有变化时标记 dirty 并上传。静止实体没有开销。`markDirty` 在下一次提取前阻止记录旧值，因此 `historyEpoch` 已删除。

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

## 实施记录

第 1 步（`RenderContext` 延迟释放与批量 copy）单独合入。第 2～5 步合并为一次修改：帧快照同时承担上传、资产保活和历史提交，分开实施需要为旧接口写临时适配。第 6 步待 profile 决定。

## 验证

- GPU 读回：首帧整表上传、未改动的表跨帧保留、多次 update 累积、取消帧、删除与 索引 复用后，常驻 buffer 与 RenderScene 一致；资产离开 Ready 集合后 GPU 表中的记录失效。
- 延迟销毁：命令列表仍在录制时释放的纹理、Mesh 和材质，在提交完成前保持存活。去掉该判断或历史提交时，对应测试失败。
- 运动历史：取消帧不推进历史，teleport、相机 cut 和 索引 复用使用 previous=current。
- D3D12 GPU validation 下测试和 Bistro 均无错误。
- 10 万实体、每帧移动 1%（1000 个分散 range）的压力测试：首帧整表上传约 190 ms，稀疏更新每帧约 60 ms，其中约 50 ms 是 `RenderScene::update` 遍历全部实体的提取，与是否有变化无关。下一步优化应针对提取，而不是上传。数值仅作参考，不作为断言；替换前的实现没有可比的测量。
