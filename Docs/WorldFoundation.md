# World Foundation

[TOC]

本文记录 World Foundation 的设计约束：资产身份与加载、ECS World 与层级、World 存档，以及 World 到 `RenderScene`/`GpuScene` 的提取。入门讲解见 [World Tutorial](WorldTutorial.md)。

World Foundation 是 GPU-driven geometry、light culling、path tracing 和 Editor 的共同数据基础。

## Glossary

类型按数据流排列。UE 一列只表示最接近的概念，语义不完全相同。

| 名称 | 含义 | UE 对应 |
| --- | --- | --- |
| `AssetID` | 资产的持久身份，128 位随机值，路径移动和重新 cook 不变 | `FPrimaryAssetId` / `FSoftObjectPath` |
| `AssetCatalog` | 磁盘资产索引：AssetID → 类型、位置、依赖 | Asset Registry |
| `AssetManager` | 运行时加载、引用计数和 payload 生命周期 | Asset Manager / Streamable Manager |
| `AssetHandle` | AssetManager 中一条记录的运行时句柄（index + generation） | `FStreamableHandle` |
| `SceneAsset` | cook 后的 glTF 节点模板，只读，可实例化进 World | Packed Level Actor / Level Instance 引用的关卡 |
| `MeshAsset` / `MaterialAsset` / `TextureAsset` | 共享的网格、材质、纹理 payload | `UStaticMesh` / `UMaterialInstance` / `UTexture2D` |
| `Submesh` / `SubmeshID` | Mesh 内一个 glTF primitive 及其稳定 ID | Mesh Section |
| `World` | ECS 运行时世界，内部用 Flecs | `UWorld` |
| `Entity` | World 中的运行时实体（带 generation） | `AActor`（大致） |
| `Guid` / `EntityIdentity` | 实体的持久身份及保存它的组件；World 文件 ID 也用 `Guid` | `FGuid`（ActorGuid） |
| `LocalTransform` / `Matrix4` | 局部 TRS / 不可分解的局部矩阵，二者互斥 | RelativeTransform |
| `WorldTransform` | 派生的世界矩阵，不保存 | ComponentToWorld |
| `MeshRenderer` | 组件：Mesh 的 AssetID + 按 SubmeshID 的材质覆盖 | `UStaticMeshComponent` |
| 五种灯光组件 / `Camera` | 光源、相机组件 | `ULightComponent` / `UCameraComponent` |
| `WorldArchive` | `.world.json` 的读写、capture 和 instantiate | `.umap` 包；Bevy `DynamicScene` |
| `TypeRegistry` | 组件和属性的反射 metadata | `UClass` / `FProperty` |
| `WorldAssetBindings` | 把 MeshRenderer 的 AssetID 解析为长期持有的资产引用 | 组件创建 SceneProxy 时的资源解析 |
| `WorldChangeLog` | 记录组件、层级或世界变换变化的实体，供 RenderScene 和资产绑定增量更新；溢出时退回全量扫描 | — |
| `RenderScene` | 渲染侧唯一的 CPU 场景数据，从 World 提取 | `FScene` |
| `RenderPrimitive` | 一个 MeshRenderer 的渲染记录：Mesh 和实例范围 | `FPrimitiveSceneInfo` |
| `RenderInstance` | 一个实例的变换、包围盒和 Guid | `FInstanceSceneData` |
| `RenderMeshBatch` | RenderPrimitive × submesh，带解析后的材质 | `FMeshBatch` |
| `RenderLight` / `RenderCamera` | 提取后的光源、相机记录 | `FLightSceneInfo` / `FViewInfo` 的输入 |
| `GpuScene` | RenderScene 和资产表在 GPU 上的唯一常驻副本 | `FGPUScene` |
| `GpuDrawList` | 每帧 draw 数据和按 `DrawListFilter` 注册的 indirect 列表 | `FMeshDrawCommand` / SRP renderer list |
| `GpuAssetTables` | Mesh、顶点流、submesh、材质、纹理的 GPU 间接表 | GPUScene 的 primitive/material 数据 |
| `GpuMeshID` / `GpuMaterialID` / `GpuTextureID` | 资产表中的强类型索引，复用 AssetHandle 的 index 与 generation | — |

| 术语 | 含义 |
| --- | --- |
| payload | loader 产出的资产运行时数据，例如 MeshAsset 及其 GPU Buffer |
| 资产管理项 / 索引 | AssetManager 或 RenderScene 中的一个存储位置；复用时 generation 递增，旧句柄失效 |
| sparse 索引 / active list | 稳定但有空洞的 索引 数组，以及当前有效 索引 的紧凑索引列表；遍历走 active list |
| `instanceIndices` | RenderPrimitive 实例范围到 RenderInstance 索引 的间接索引 |
| 长期引用（lease） | WorldAssetBindings 为活着的 MeshRenderer 持有的资产引用，组件删除时释放 |
| pending uploads | RenderScene 中上次上传以来改动的 索引，跨多次 update 累积，`GpuScene::EndFrame` 清除 |
| staging | 每帧写入的 CPU_TO_GPU 上传缓冲，按提交轮换，经批量 copy 写入常驻表 |
| 延迟释放 | `RenderContext::DeferRelease` 和 AssetManager 都在可能使用资源的提交完成后才销毁它 |
| current / previous | 本帧与上一次实际提交的变换；只有提交的帧推进历史，用于运动矢量 |
| sidecar | 源文件旁的 `.asset.json`，保存导入资产的持久 ID |
| contentHash | 源文件、导入设置和 cooker 版本的指纹，只判断是否需要重新 cook，不是身份 |
| PayloadCompatible / StructuralChange | hot reload 的两类变化：只换 payload，或 submesh、bounds、layout 改变需要重建渲染记录 |

## TODO: OpenUSD alignment

后续资产格式以标准 OpenUSD 为基准，Runtime 保留轻量组件，通过导入层映射 USD 语义。当前先完成 World 的通用能力，暂不接入 USD。以下为待办，尚未实现；本文现有 Light 单位约定、glTF 导入方案和 `.world.json` 保存方案需据此复核。

- [ ] 明确 USD 是否同时承担场景保存，以及自定义 cooked 格式是否仅作为运行时缓存；确定后更新序列化与实施步骤。
- [ ] USD 导入时将 UsdLux 的单位、发射形状和坐标轴映射到对应的灯光组件。
- [ ] 扩展 Camera：投影类型、焦距、片门尺寸及偏移、光圈、对焦距离、快门时间；FOV 从镜头参数派生。
- [ ] 完善 Transform/Parent：支持 resetXformStack；导入层求值 xformOpOrder 和 pivot，保留无法分解的局部矩阵。
- [ ] 新增 Visibility/Purpose：处理继承可见性和 default/render/proxy/guide 用途。
- [ ] 完善 MeshRenderer 材质映射：解析整 mesh、面子集、继承和 collection 绑定；拓扑、primvars、细分参数归 MeshAsset，材质网络归 MaterialAsset。
- [ ] 增加实例表达：区分原生场景实例与 PointInstancer，支持 prototype 引用、实例 ID、变换数组和隐藏状态。
- [ ] 后续补充骨骼、蒙皮、BlendShape、动画与几何缓存。
- [ ] 在 Stage／导入层统一场景单位、坐标轴、颜色空间和时间基准；明确 USD prim 与 AssetID/Guid 的身份映射及回写边界。

## Goals

- 资产具有独立于路径和运行时 索引 的持久身份。
- Asset Manager 使用独立 typed loader pipeline。
- Entity、持久化实体和 GPU instance 使用不同的 ID 与生命周期。
- World 使用 data-oriented ECS 存储组件，Flecs 类型不暴露在 Runtime 公共接口中。
- Transform hierarchy 支持动态创建、删除和 reparent，并只更新 dirty subtree。
- World 文件可确定性保存、加载和逐版本迁移。
- Reflection metadata 同时服务序列化、Editor Inspector 和未来 undo/redo。
- Renderer 只读取帧稳定的 `RenderScene`，不直接查询或持有 ECS storage。
- Asset 与 GPU 资源销毁遵守异步加载和 GPU fence 生命周期。
- Render extraction 热路径不进行内存分配。

## Non-goals

当前不实现完整 Editor、prefab、scripting、network replication、自动多线程 ECS 调度、实例级 GPU 剔除、path tracing 或分布式 cooking。设计需要为这些功能保留稳定边界，但不提前实现其内部机制。

## Architecture

```text
AssetPipeline -> Cooked Asset
                       |
                 AssetManager
                       |
                    ECS World
                       |
              RenderScene Extraction
                       |
                    Renderer

Editor -> Commands -> ECS World
```

这些能力统一属于现有 `Scene` Runtime 模块，内部职责如下：

- `Scene/Assets` 管理资产目录、身份、依赖、typed loaders、加载状态和运行时资源所有权。
- `Scene/World` 管理 Entity、Component 和 hierarchy。
- `Scene/Reflection` 管理 type/property metadata。
- `Scene/Serialization` 管理 authoring World 保存、加载和迁移。
- `Scene/RenderScene` 管理渲染快照、sparse 索引（+active list）和 GPU 更新范围。
- `ResourceLoader` 仍可承担底层 GPU upload，但其 token 和资源指针不成为 Asset Manager 的公共契约。

不新增平级 Runtime 模块。公共接口放在 `Source/Runtime/Scene/Public/Scene`，实现按 `Assets`、`World`、`Reflection`、`Serialization` 和 `RenderScene` 子目录放在 `Source/Runtime/Scene/Private`。Editor 后续作为独立 Tool target，只依赖 Scene 的公共接口。

第一方接口优先使用轻量 OOP：World、Asset Manager 和 RenderScene 由类封装状态与生命周期，资源所有权使用 RAII；ID 的生成和转换作为类型自身的方法。Component、metadata 和配置描述保留简单结构体，typed loader 继续使用函数表，不为统一接口引入继承或虚函数层次。

## Identity model

| Identity | Purpose | Lifetime |
| --- | --- | --- |
| `AssetID` | 标识 Mesh、Texture、Material、SceneAsset 等磁盘资产 | 跨路径移动、进程和 World 保存稳定 |
| `AssetHandle` | 定位 Asset Manager 中的一次运行时实例 | Asset Manager 索引 生命周期，包含 generation |
| `Guid` | World 文件、Editor selection 和跨实体引用 | 跨 World 保存稳定 |
| `Entity` | ECS 中的运行时实体 | 当前 World 生命周期，包含 generation |
| `RenderPrimitiveID` | RenderScene 中一个 MeshRenderer 的 mesh 级记录（material overrides + instance range） | 不持久化；sparse 索引 + generation，回收后可变 |
| `RenderInstanceID` | 一个可渲染实例的 transform/bounds/identity | 不持久化；同上 |
| `RenderMeshBatchID` | RenderPrimitive × Submesh 的 draw 记录 | 不持久化；随 primitive/submesh 派生 |

`AssetID` 和 `Guid` 使用 128-bit 值，采用随机生成（UUIDv4 风格），不由内容或路径派生，因此重命名、移动或重新 cook 都不改变身份。碰撞概率可忽略，注册时仍校验唯一性并在冲突时报错而非静默覆盖。`AssetID` 由 importer 生成并写入 sidecar metadata，移动源文件时保持不变。`Guid` 在创建 persistent entity 时生成。

`World::CreateEntity` 创建无持久身份的运行时实体；`CreatePersistentEntity()` 自动生成 Guid，`CreatePersistentEntity(id)` 接受导入或恢复的非空 ID。ID 在每个 World 内唯一，冲突返回失败，不替换已有实体；可通过 World 构造参数注入确定性的 IDGenerator。EntityIdentity 创建后只读，禁止通过普通组件接口增删或改写。

`GetEntity(id)` 和 `GetEntityGuid(entity)` 查询已提交的身份。Defer 中的新 ID 立即保留以拒绝重复创建，但查询在最外层提交后才可见；已有实体的延迟删除也在提交时移除映射。创建后立即取消同样释放保留项。删除父节点保留的子实体继续使用原 ID；显式恢复同一 Guid 时会得到新的 runtime Entity generation。

测试和工具需要确定性时，通过显式注入的种子/ID 序列生成，而不是依赖全局随机源；这样 `.world.json` 的连续保存和回归测试可复现。

现有 `contentHash` 继续表示 source、import settings 和 cooker revision 的组合指纹。它用于判断是否需要重新 cook，不能作为资产身份。

运行时 handle 统一使用 index 加 generation。无效 handle 使用 `UINT32_MAX` 索引或零 generation。

## Asset Manager

Asset 系统分成三个部分：

- Asset Catalog 保存 `AssetID` 到 type、source URI、cooked URI 和 dependency IDs 的确定性映射。
- Asset Manager 保存运行时 索引、状态、引用计数、payload version 和错误信息。
- Typed Loader 负责特定资产类型的 IO、CPU decode、GPU upload、publish 和 destroy。

Catalog 可以在启动时从 cooked index 加载，也可以由 Editor 增量更新。Asset Manager 不扫描磁盘来猜测资产身份。每个运行时 资产管理项 引用 Catalog entry，但不复制完整依赖和 manifest 数据。

状态转换为：

```text
Unloaded -> Queued -> LoadingCPU -> WaitingDependencies -> UploadingGPU -> Ready
              |             |               |                  |           |
              +-------------+---------------+------------------+-----------+
                                            |
                                            v
                                          Failed

Ready/Failed -> Retiring -> Unloaded
```

主要约束：

- 重复请求同一 `AssetID` 时复用 资产管理项，不重复加载资源。
- dependency graph 在排队阶段检测循环依赖，父资产只有在必要依赖 ready 后才能进入 ready。材质的纹理是可选依赖：纹理失败时材质仍然 ready，GPU 表中该槽位保持未绑定，不再让 mesh 和场景整体失败。
- request 和 release 不隐式等待 IO 或 GPU upload。
- loading 期间的最后一次 release 将 资产管理项 标记为 retiring；loader 完成后再安全清理。
- GPU payload 只能在相关 GPU 工作完成后销毁。
- 加载失败返回明确状态，并允许 renderer 使用 placeholder。
- Runtime 使用虚拟资源路径，不将绝对路径保存到 Asset Manager 或 World。
- Asset Manager 更新集中在明确的 frame phase；后台线程只发布完成结果，不直接修改 World。
- 每个异步请求携带 索引 generation 和 cancellation state；迟到的完成消息不能写入已经复用的 索引。
- CPU decode 失败、dependency 失败、GPU upload 失败和 cancellation 都必须进入同一清理路径。
- publish 是原子的；消费者只能看到旧 payload 或完整的新 payload，不能看到部分初始化状态。
- hot reload 创建新的 payload version。旧版本继续服务正在执行的帧，并在引用和 GPU fence 都完成后回收。
- shutdown 先停止接收请求，再取消或 drain 后台工作，最后等待并销毁已发布的 GPU payload。

状态机中 `WaitingDependencies` 与 `LoadingCPU` 的关系：依赖在进入 `Queued` 时即被**预留**（提高引用计数、触发其加载），本资产完成自身 CPU decode 后进入 `WaitingDependencies` 等待这些依赖 ready，再进入 `UploadingGPU`。即"预留在前、等待在后"，二者不冲突。

Typed Loader 使用显式注册的函数表，不依赖 RTTI。一个加载事务依次执行：解析 Catalog entry、预留依赖、后台 IO/CPU decode、提交 GPU upload、等待 upload token、发布 payload。PayloadCompatible 结果可以直接原子发布；StructuralChange 只提交 prepared candidate，由 frame-boundary transaction 与 RenderScene 更新一起发布。失败时按相反顺序清理已经取得的资源。Texture、Mesh、Material 和 SceneAsset 从第一版开始就是独立 loader；SceneAsset payload 只保存可实例化的 node/component 模板和其他 `AssetID`，不拥有其依赖资产的 GPU payload。

Asset Manager 可以在 loader 内部调用现有 ResourceLoader，但负责屏蔽其资源指针、callback 和同步细节。该边界允许未来替换 IO、加入 streaming 或接入 Editor asset daemon，而不改变 World 和 Renderer。

**GPU 资源所有权、稳定 GpuID 与间接层**：这里刻意与 RHI 的 descriptor 契约解耦——descriptor index 由 **RHI 在资源创建时分配**、随资源生命周期稳定（见 `Docs/Bindless.md`：texture 的 SRV/UAV、buffer 的 SRV、material 作为 table element、sampler 独立 heap，各自不同，**不存在单一的"资产 bindless index"**）。因此分层如下：

```text
AssetID
  → Asset payload（typed loader 拥有）
    → GpuMeshData / GpuMaterialData / GPUTexture
      → resource-specific descriptor indices（由 RHI 分配/回收）
```

- Asset Manager 只拥有 **typed payload 生命周期**，不自己管 descriptor 分配；RHI 继续拥有 descriptor allocator。
- Asset Manager 维护版本化的 GPU 间接表，例如 `GpuMeshTable[GpuMeshID] → 当前 mesh payload（含各 submesh 的 index/vertex descriptor）`；Material 和 Texture 使用各自的 `GpuMaterialTable` 与 `GpuTextureTable`。这些 GpuID 不是裸 descriptor index。
- `GpuSubmeshRef` 是运行时组合引用 `{GpuMeshID, submeshIndex}`，不是独立 ID。它只在创建它的 mesh layout 和对应上传的表内有效；发生 structural reload 后必须重新解析稳定 `SubmeshID`，不能把旧 `submeshIndex` 当成持久身份。
- RenderMeshBatch 保存 `GpuSubmeshRef`/`GpuMaterialID`，不保存 descriptor index 或裸指针。resolve 时先由 `GpuMeshID` 得到本帧上传的表中的 mesh payload，再以 `submeshIndex` 取得 submesh descriptor。
- **hot reload 分两类，不能一概而论"RenderScene 无 dirty"**：
  - **PayloadCompatible**（layout 与 bounds 不变，只替换纹理像素、材质常量等）——只更新间接表里该 ID 指向的 payload version，RenderScene 不用 dirty。旧 payload 退休后，在其版本使用者全部释放且相关 GPU fence 完成时销毁，不等待长期 asset lease 清零。
  - **StructuralChange**（submesh 数量/稳定映射、bounds、默认材质、vertex/index layout 改变）——Asset Manager 产生失效信息，resolve/binding 层根据稳定 `SubmeshID` 重建受影响的 RenderMeshBatch 和 bounds。这类 reload 无法对 RenderScene 透明。
  - **texture reload** 要求 Material table 存的是 `GpuTextureID`（经间接表解析），不是旧 texture descriptor；否则父 Material 不 reload 就会引用已退休的 descriptor。
- StructuralChange 的 publish 是一个 frame-boundary transaction：先准备新 payload，根据稳定 `SubmeshID` 重建 binding、RenderMeshBatch 和 bounds，最后在同一次 GPU 上传中一起 publish。旧帧在队列上先于这次上传执行，继续使用相互匹配的旧场景记录、旧 table 和旧 payload，禁止出现“新 table + 旧 RenderMeshBatch”的中间状态。
- `GpuMeshID`、`GpuMaterialID` 和 `GpuTextureID` 都是运行时 `{index, generation}` handle，在对应 Asset Manager 资产管理项 驻留期间跨 payload reload 稳定。只有长期 asset leases 和相关 GPU fences 都结束后才能回收 table 索引；复用时递增 generation。CPU 在编码上传的表时验证 generation，GPU 只消费本帧表中已经验证过的 table index，因此旧 ID 不会在执行中的帧内别名到另一份资产。

**三层引用生命周期（必须区分，否则活着的 MeshRenderer 引用的资产会被卸载）**：

1. **SceneAsset 实例化期的 dependency lease**：只在"加载 SceneAsset → spawn entities"这段事务里持有，是**瞬态**的。SceneAsset 资产管理项 一旦被释放（它的用途只是产出 entity），这些 lease 就释放——**它不能作为运行期常驻引用的持有者**。
2. **运行期长期 lease（本文新增的关键持有者）**：由 Scene 的 **resolve/binding 层**为**每个活跃的 `MeshRenderer`** 持有一份对已解析 Mesh/Material 的长期 asset lease；component 删除（或 Entity 销毁）时释放。这才是"World 里 MeshRenderer 存活 ⇒ 资产常驻"的保证。因此 `MeshRenderer` **只存 `AssetID`（不存 `AssetHandle`）**——handle 是 Asset Manager 索引 生命周期，绝不能泄漏进 ECS 和序列化。
3. **GPU 使用期**：引用归零后，Asset Manager 记下当时可能使用该 payload 的最后一次提交，fence 完成后才销毁；不按 RenderMeshBatch 或 instance 增减引用计数（见 Threading）。

长期 asset lease 保活的是 **资产管理项 和当前 payload**，不固定历史 payload version。当前版本通过 资产管理项 持有依赖 lease；reload candidate 先取得自身所需依赖，发布后旧版本所需的依赖仍保留到该版本退休清理完成。旧版本的回收只等待其版本使用者和相关 GPU fence；资产管理项 与 GpuID 索引 的回收才要求长期 lease 清零、后台工作结束且所有版本完成清理。Payload 只保存 `AssetID` 或已解析的稳定 GpuID，不保存依赖 GPU 指针。依赖的 hot reload **不**自动重载父资产，除非父 loader 显式声明"依赖版本会影响自身 payload"。

Asset metadata 初期使用 sidecar 文件保存 `AssetID`、type、source URI 和 cooked URI。Cooker 输出继续保存 content hash 和依赖信息。

## SceneAsset 实例化

SceneAsset 与 World 采用 **node → Entity** 映射：

- SceneAsset payload 保存一棵 **node 模板树**：每个 node 含**局部变换（tagged：`LocalTransform` 或 `Matrix4` 二选一——glTF node 本身可用 matrix 形式，不可分解时保留为 matrix）**、parent 索引，以及（对有几何的 node）一个 `MeshRenderer` 模板。它不保存烘焙后的世界矩阵，也不拥有依赖资产的 GPU payload。
- **一个 node 对应一个 Entity，而不是一个 primitive 一个 Entity。** glTF 一个 mesh node 可含多个 primitive、每个 primitive 有独立材质。因此这些 submesh 归到一个 `MeshAsset` 里，由**一个** node Entity 的 `MeshRenderer` 引用；不为每个 primitive 建独立 World Entity——除非该 primitive 确需独立 transform 或编辑身份。
- `MeshRenderer` 结构：`mesh: AssetID` + **material override set（稀疏）**。`MeshAsset` 内保存 submesh/primitive 表和每个 submesh 的**默认 material 绑定**；`MeshRenderer` 只记录与默认不同的槽（天然对接未来 prefab override）。读取时 default + override 合并。
- 实例化时为每个 node 创建 Entity，重建局部变换 + `Parent` 层级，并在其上挂一个 instance root Entity，使整份场景可作为一个单位被移动或 reparent；**删除用 `DestroySubtree(instanceRoot)`**（见 Transform hierarchy），保证整棵子树一起删、不留孤儿节点。
- 同一 SceneAsset 多次实例化时 node 各自独立成 Entity，但引用的 Mesh/Material 通过 `AssetID` 复用同一份依赖 资产管理项（多个 Entity 共享这些资产 资产管理项）。
- **子资源身份分两类**：submesh 使用 `MeshAsset` 内部的稳定 `SubmeshID`（不是全局 `AssetID`）；导出的 Mesh、Material 和内嵌 Texture 都是独立资产，各有 `AssetID`。源文件 sidecar 保存 SceneAsset ID 和完整子资产身份表，包括 type、持久 ID、source 匹配信息，以及每个 Mesh 的 SubmeshID 表。引用已独立注册的外部 Texture 时复用其 AssetID。
- **重导入统一匹配规则**：首次导入分配并持久化上述 ID；重新导入优先使用 exporter/source 提供的持久身份，名称、几何和材质签名仅用于辅助唯一匹配，数组索引和内容 hash 不能作为身份。插入、重排或重命名后，能唯一匹配的资源沿用 ID；歧义项分配新 ID 并报告。被删除或无法匹配的旧条目保留为 missing，不将其 ID 分配给其他资源；已有 World 引用或 material override 保持原 ID 并报告未解析，不能静默改绑。Cooked 引用与 sidecar 身份表必须一致后才能发布 Catalog。

Cooked SceneAsset 只支持当前格式和版本，不做旧格式识别、转换或兼容加载。Cooker 输出 node 层级、局部变换、MeshAsset 引用及其 submesh/default-material 表。

World 内部使用 Flecs。公共 `Entity` 是轻量的 64-bit value，Flecs world、query 和 component ID 只存在于 `World/Private`，避免第三方接口扩散到 Runtime、Editor 和 Renderer。

第一批组件为：

- `EntityIdentity`
- `Name`
- `LocalTransform`（TRS）
- `Matrix4`（可选，见 Transform hierarchy；import 用它承载不可分解的仿射变换）
- `Parent`（公共 API 视图，内部由 World 私有的 Flecs 排他关系承载，非独立真相来源）
- `WorldTransform`（派生数据，不进 World 文件，存于层级有序存储）
- `MeshRenderer`
- `DirectionalLight`、`PointLight`、`SpotLight`、`RectLight`、`SkyLight`
- `Camera`

Component 保存 authoring 或 simulation 数据，不拥有 GPU resource。`MeshRenderer` **只保存 `mesh: AssetID` 和 material override set**——不保存 `AssetHandle`、`Geometry*`、`GPUBuffer*`、descriptor index、`GpuMeshID` 或 RenderScene 索引（这些都是运行期状态，由 resolve 层维护并持有长期 lease，见 Asset Manager 一节）。

System 管理行为，例如 transform propagation 和 render extraction。当前采用确定性的单线程更新顺序；query 迭代期间的结构变化进入 deferred command buffer，并在 phase boundary 应用。多线程调度在 World contract 和单线程结果稳定后再接入。

当前 `World`、`WorldQuery` 和 `DeferredChanges` 使用 RAII，内存接入 tf allocator。World 在构造时注册 TypeRegistry 中已有的类型，metadata 必须比 World 活得更久。查询只提供组件的只读视图，缓存查询应复用；结构修改在最外层 defer 作用域结束时应用。沿用 Flecs 的语义：`setComponent` 对已有组件立即更新值，对尚不存在的组件保存独立副本并延后添加，因此 defer 不提供值的快照隔离。需要覆盖整个更新阶段时，由调用方持有外层 defer 作用域。

### Flecs 复用边界

Flecs 自带 meta（反射）、JSON 序列化、`ChildOf` 关系和 deferred 操作。为避免重复造轮子又不让第三方类型泄漏，划清复用 / 自建边界：

- **复用 Flecs**：component storage、query、pair 关系作为 `Parent` 的内部存储、以及 deferred command queue。这些是 Flecs 的强项，且不进入公共接口。
- **自建（不使用 Flecs meta / JSON）**：反射 type/property registry、`.world.json` 序列化、component migration。原因是本项目对这三者有 Flecs meta/JSON 不满足的硬需求——**确定性输出**（稳定排序 + 固定 float 格式，用于 diff 和回归）、**逐版本 schema migration**、**同一份 metadata 复用于 Editor Inspector 与未来 undo/redo command**，以及**不把 Flecs 类型暴露到 Editor/Renderer**。用 Flecs JSON 会把其数据模型和格式细节固化进 World 文件，迁移和 Editor 掌控都受限。
- **不自建**的部分（如 hierarchy 存储）直接用 Flecs pair，只在 Public 层用不透明 `Parent`/`Entity` 封装。私有父子关系不启用继承式遍历；World 使用自己的迭代更新，避免深层编辑触发 Flecs 的递归事件传播。

该边界让"隐藏 Flecs 类型"这个目标不等价于"重造 ECS"，把自建工作量收敛到确实需要自控的三块。

## Light authoring

World 为方向光、点光、聚光、矩形光和天空光分别注册组件。Inspector 和世界存档使用各类型的反射字段。同一实体可挂不同类型的灯，共用实体变换，各自拥有独立的渲染句柄。旧版通用 `Light` 组件没有迁移器。

组件采用 UE 的可编辑字段和单位。点光、聚光和矩形光通过 `AttenuationRadius` 指定截止距离，默认 1000 cm，设为零时不参与着色。方向光强度单位是 lux。颜色使用场景线性 RGB，组件角度使用度。场景变换使用米，RenderScene 在提取时转换尺寸和角度。

Spot 复用 Point 的光源半径和长度，Rect 使用矩形宽高，Sky 使用环境光强度、颜色和立方体贴图。主方向光的 `LightSourceAngle` 和 `ShadowSourceAngleFactor` 控制光追阴影的软硬。局部光统一使用平方反比衰减。

Lightmass、IES、云阴影、自定义衰减指数和天空贴图旋转角保存在组件和世界存档中。当前 shader 使用立方体贴图的原始朝向。矩形光保留 `SourceTexture` 引用，当前矩形着色按均匀发光面求值。

## Transform hierarchy

**Local 变换有两种表示，二者严格互斥**：`LocalTransform`（TRS：translation/rotation/scale）是 authoring 首选；`Matrix4`（可选组件）承载**无法无损分解为 TRS** 的仿射变换。原因：父级非均匀缩放与旋转组合后，world/local 矩阵可能含 **shear**，TRS 无法精确表达；负缩放也有歧义。**互斥由 World API 强制**——一个 Entity 二者只能有其一；加载到同时带两者的**非法文件直接报错**（不做"谁优先"的隐式规则）。importer 对不可分解的局部变换使用 `Matrix4`，作者手编或可分解的节点使用 `LocalTransform`。`Parent` 表示层级关系；`WorldTransform` 保存当前世界矩阵，属于派生数据，不写入 World 文件。运动历史只由 RenderScene 保存。

Hierarchy 必须满足：

- `SetParent` 拒绝 self-parent 和 cycle。
- `SetParent(..., keepWorld=true)` 尝试解 `newParentWorld · local = childWorld`。不实现奇异矩阵的广义求解：只要 new parent world 不可逆就返回失败并保持原 parent。
  - **可解**：结果可分解则写 `LocalTransform`，否则写 `Matrix4`（保留精确仿射结果，绝不有损 TRS 近似）。
- `SetParent(..., keepWorld=false)`：正常 reparent，直接沿用现有 local，不受上面限制。
- **两种删除操作，语义必须分开**：`DestroyEntity(e)` 删单个 Entity，其 children **reparent 到 root 并保持 world transform**（root=单位阵、恒可逆，keepWorld 恒可解，必要时落 `Matrix4`）；`DestroySubtree(e)` 删 e 及**整棵 transform subtree**。**SceneAsset instance 的删除用 `DestroySubtree`**（删 instance root = 删整份 Bistro），否则会把节点遗留在 World。
- `DestroyEntity` 先保存并解除直接子节点关系，再删除父节点；`DestroySubtree` 按叶到根顺序删除，显式控制删除范围。私有父子关系保留单父和目标删除策略，不修改 Flecs 内建关系。
- local 或 parent 变化只标记受影响 subtree。
- 更新使用迭代式 root-to-leaf 顺序，不依赖递归调用栈。
- teleport 或 camera cut 可以显式同步 previous transform，避免错误 motion vector。
- Transform update 在 RenderScene extraction 前完成。

**单一真相 + 派生遍历 cache**：层级的**唯一权威来源是 World 私有的 Flecs 父子关系**（`Parent` 只是它的公共视图，不是第二份存储）。Flecs 按 archetype/table 存储，普通 query 迭代**不是**层级顺序，也无法便宜地"只遍历 dirty subtree"。因此从父子关系**派生**一个层级遍历 cache——parent 恒在 child 之前的拓扑序（DFS）数组，结构变化后重建，旧的节点索引和缓存指针随之失效。propagation 沿该数组从前往后单次线性扫描；dirty 用 dirty-root 队列表示，重建后每个 subtree 是数组里一段**连续 DFS range**，clean subtree 直接跳过。`WorldTransform` 作为派生数据存在这套 dense、hierarchy-ordered 存储里，而不是靠普通 component query 逐个更新；它不进 World 文件。

**结构变化（create/delete/reparent）时全量 O(N) 重建该 cache**，不追求增量拓扑维护。理由：要同时保住"parent 在前 + subtree 连续可跳过 + reparent 不移动数据"三者，数学上做不到（reparent 必然打乱 DFS 连续性）；而结构变化通常远少于 transform 更新，第一版用重建换正确性和简单性，增量维护留到有 profile 证据再做。

当前接口：`SetParent` 返回是否成功；`GetParent` 提供父子关系的公共视图。Reparent 和删除带子节点的实体必须在 query/defer 作用域外调用，否则返回失败；叶节点增删与局部组件修改仍可 defer，LocalTransform/Matrix4 互斥检查同时覆盖待提交操作。删除父节点需要 TypeRegistry 注册 Matrix4，以便保留无法分解的结果。

`UpdateTransforms` 在结构修改后重建缓存，普通更新只遍历排序、合并后的脏子树；新建实体追加为根，删除叶子留下空位，只有重新挂接父子、延迟作用域内的结构修改或空位过多时才重建。缓存预热后，稳定层级的变换更新不分配内存。读取 `GetWorldTransform` 前必须完成更新。World 不保存运动历史：RenderScene 保存实际提交的历史，渲染路径在 `GpuScene::EndFrame` 推进历史，teleport 或 camera cut 通过 `RenderScene::MarkDirty` 重置。

后续并行化时以独立 root 或预计算的 hierarchy range 为任务单位，结果必须与单线程路径一致。

## Reflection and property metadata

Reflection 使用显式 type registry，不依赖 C++ RTTI、异常或复杂模板实例化。组件在 Runtime 初始化期间显式注册，不能依赖跨模块静态初始化顺序。

每个 component descriptor 包含稳定 type ID、名称、schema version、size、alignment 和 property span。相邻版本的 migration callback 由存档加载入口传入，反射接口不依赖 JSON 库。含 owning collection 的 component 还必须注册显式 init/copy/move/destroy lifecycle operations，并由 World 转接为内部 Flecs hooks，保证 archetype 移动、deferred command 和 World 销毁不会浅拷贝或泄漏 collection storage。每个 property descriptor 包含稳定 property ID、名称、类型、offset、size、flags，以及 Inspector 所需的 range、step 或 widget hint；集合 property 还要引用 element descriptor 和统一的 collection operations。

第一版支持以下 property 类型：

- boolean、signed/unsigned integer 和 float
- vector、quaternion 和 color
- bounded string 和 enum
- `AssetID` 与 `Guid` reference
- **array-of-struct**：元素是固定 schema、可平凡复制的小结构体，字段仍由上述标量/ID property 描述。复用现有 `hz::Array` 及 tf memory，不新增 Scene 容器或 allocator；metadata 提供 element descriptor 和统一的 `ArrayOperations`，完成 count/data 查询及 resize/insert/remove，消费者不依赖数组内存布局。Serializer、Inspector 和 undo/redo 不直接缓存 data pointer，所有 mutation 都经过 World property API，以便产生 dirty/change notification。

World 属性编辑只修改已有组件，校验属性 kind、字节大小、只读标记、枚举值、字符串终止符和数组索引；失败保留原值。字符串输入长度包含结尾空字符。数组插入接收反射元素类型的完整实体，元素字段编辑遵守自身只读标记。字段赋值不复制整个组件或分配数组存储；扩容可能使数组数据指针失效。defer 中已有组件的属性立即更新，新添组件需等待提交。`setComponent` 仍是可信调用方的整组件替换入口，不提供逐字段校验；`EntityIdentity` 只能通过实体创建接口设置。

`MeshRenderer` 的 override array 元素为 `MaterialOverride { SubmeshID, materialAssetID }`。后续专用 override mutation API 需保持该数组按 `SubmeshID` 排序且 key 唯一；当前通用数组编辑保留用户指定顺序，不承担该约束。loader 在 staging World 中验证并规范化输入，serializer 直接按存储顺序输出，不在保存热路径复制或排序数组。

Property flags 至少区分 serialized、transient、read-only 和 editor-only。注册时检查重复 ID、名称冲突、越界 offset、不完整的 array element metadata 和不支持的字段类型。

同一份 metadata 服务以下消费者：

- World serializer
- Editor Inspector
- 未来 command/undo/redo
- prefab override
- property animation 和 remote control

当前手工声明少量 descriptor；类型规模增加后再引入代码生成。不能为每个消费者维护一套独立字段表。

## World serialization and migration

可编辑 World 使用 `.world.json`，与 cooked SceneAsset 区分。

World 文件保存：

- file schema version 和 World ID
- 按 `Guid` 稳定排序的 entity 资产管理项
- entity name 和 parent `Guid`
- component name、component version 和 serialized properties
- `AssetID` reference，不保存 runtime handle、pointer 或 GPU index

加载使用分阶段提交：

1. 验证顶层 schema，并创建 staging World。
2. 创建全部 Entity，建立 `Guid` 映射。
3. 添加和迁移 Components。
4. 解析 parent 与其他实体引用。
5. 验证 hierarchy 并更新 transforms。
6. 全部成功后原子替换目标 World。

任何失败都不能留下部分修改的目标 World。输出顺序和 float formatting 必须确定，使相同 World 连续保存得到相同内容。

Component 独立维护 schema version，migration 按相邻版本顺序执行，例如 `v1 -> v2 -> v3`。纯 Runtime 加载遇到未知 component 或无法迁移的版本时返回明确错误。但**文件格式从第一版就必须允许保留未知 component 的原始 JSON blob**：否则 Editor 打开由更新版本或新插件创建的 World 会无法恢复、round-trip 丢数据。保留原始 blob 是格式层的硬要求；Editor 侧"读取-透传-回写"未知数据的完整实现可延后，但格式不能事后再加。

Cooked World 将来可以使用分块二进制格式，但不能直接 dump ECS 内存布局。

当前入口为 `WorldArchive`：`Capture` 从已提交的 World 提取 persistent entities，`save/load` 处理文件，`Instantiate` 在成功后替换调用方持有的 `UniquePtr<World>`。替换前须销毁旧 World 的 query/defer scopes；失败保留旧 World。World ID 由存档持有，可显式指定，后续 capture 默认沿用。无 Guid 的运行时 Entity 不保存；persistent entity 的父节点必须有 Guid。

`Horizon.World` v1 按 Guid、TypeID、PropertyID 排序输出；组件记录保留稳定 TypeID、名称、版本及独立 properties 对象。64 位整数使用十进制字符串，float/double 分别使用 9/17 位有效数字的区域无关格式。只保存 serialized properties，数组沿用存储顺序；加载时规范化 material overrides，重复 SubmeshID 拒绝，保存时要求键已唯一且有序。

存档解析保留原始 JSON，因此未知组件及其数值可原样读写；Runtime 实例化仍拒绝未知组件、缺失迁移、悬空实体引用和非法层级。迁移逐版本变换 properties JSON，不改写原文档。文件保存先写临时文件，再通过备份替换，失败可恢复旧文件；这不提供断电后的持久性保证。Editor 对未知组件的编辑合并仍属后续工作。

## RenderScene extraction

RenderScene 是 World 与 Renderer 的唯一数据边界。它属于 Scene 模块，负责保存当前可渲染世界的帧稳定表示；提取发生在 update phase 完成之后，Renderer 在该帧不再访问 ECS。

**三层模型（一个 MeshRenderer Entity 会展开成多个 draw）**：一个 node Entity 的 mesh 含多个 submesh、各有材质，因此"一 instance = 一个 GpuMeshID/GpuMaterialID"不成立。RenderScene 分三层（术语上区分 glTF **submesh/primitive** 与 UE 意义的 scene **RenderPrimitive**）：

```text
MeshAsset
  └─ Submesh 资产管理项（共享静态几何；GpuSubmeshRef = {GpuMeshID, submeshIndex}）

RenderPrimitive (RenderPrimitiveID)  = 一个 MeshRenderer Entity
  ├─ resolved material overrides（default + override 合并后的结果）
  └─ instance range → instanceIndices[] → RenderInstance sparse 索引

RenderInstance (RenderInstanceID)
  └─ current/previous world transform + world bounds + Guid(picking)

RenderMeshBatch (RenderMeshBatchID)  = RenderPrimitive × Submesh
  └─ RenderPrimitiveID + GpuSubmeshRef + GpuMaterialID
```

- **transform 从第一天就在 per-instance 层（`RenderInstance`），不在 `RenderPrimitive` 上**——这样将来一个 primitive 挂多 instance（ISM/foliage）不改 buffer layout。
- **RenderPrimitive** 持 mesh 级共享数据（material overrides、instance range），一个 Entity 一份。Range 的 offset/count 指向 `instanceIndices`，不是 RenderInstance sparse 索引 的连续范围；索引元素指向已验证 generation 的 instance 索引。实例增删可以迁移索引段并更新 primitive range，不移动存活的 instance 索引；旧帧在队列上先于新的上传执行，继续读取当时的 range 和索引段。
- **RenderInstance** 持 transform/bounds/identity。**当前每个 RenderPrimitive 恰好 1 个 RenderInstance**（一 Entity → 1 primitive + 1 instance + N 个 RenderMeshBatch，N=submesh 数）。
- **RenderMeshBatch** 是 draw 模板（primitive × submesh），持本帧表内有效的 `GpuSubmeshRef` 和 resolve 后的 `GpuMaterialID`（descriptor 走 Asset Manager 间接表，不存 descriptor index）；draw 时按 primitive 的 instance range 展开。
- lights、cameras 同为 RenderScene 顶层 sparse 索引 数组，分别使用 `RenderLightID`、`RenderCameraID` 和 active list。
- 可见性**不**作为 CPU mask 存储——见下方 GPU-driven 预留；未来 GPU culling 输出的正是可见 (RenderInstance × RenderMeshBatch)（或 meshlet）列表。

主要约束：

- Entity 获得稳定的 RenderScene 索引，直到对应 render component 被移除。
- extraction 输出 created、updated 和 removed 索引，并合并连续 dirty ranges。
- 数据数组提前 reserve；正常帧的 extraction 和 GPU upload 不分配内存。
- asset 未 ready 时使用 placeholder 或跳过 instance，不阻塞渲染线程。
- 删除 Entity 时，CPU 索引 可以立即失效并复用；常驻 GPU 表中的旧记录由之后的上传覆盖，被替换的 Buffer 延迟到 GPU 用完后释放。
- **渲染历史按实际提交推进**：hierarchy update 只更新 current；extraction 的 previous 取上一次已提交帧的 current，不取上一次 Update 或 extraction 的结果。提交成功后才推进历史基准，下一次提取把 previous 的变化加入 instance dirty ranges，即使实体未移动也上传 previous=current；未提交的帧不消耗历史。首次出现、teleport、camera cut 和窗口恢复渲染后的首帧使用 previous=current；恢复时同步重置相机历史。只有 Update、连续多次 Update 或取消的帧都不推进渲染历史。
- RenderScene 不包含 Flecs 类型、authoring 路径或 Asset Manager 内部 pointer。

**索引 模型（稳定 ≠ 密集，用 sparse + active list 两层）**——`RenderPrimitiveID`、`RenderInstanceID`、`RenderMeshBatchID` 各自的数组套用同一模式：
- **stable sparse 索引 array**：id 指向一个稳定 索引（带 generation），对应 render 数据存活期间**不移动**；删除时回收进 **free-list** 供复用。因为有 free-list，数组是**稀疏的（有洞）**，不是 densely packed——Renderer 不能盲目遍历 `[0, capacity)`。
- **dense active-index list**：每层另存一个当前活跃 索引 的紧凑列表。CPU 遍历、以及未来 GPU culling，都遍历 active list，而不是稀疏数组。
- **dirty upload**：按被创建/更新的 sparse 索引 的 index range 上传（稀疏 dirty 见下方 scatter 预留）。
- `instanceIndices`、primitive range 和 active list 的修改、计数变化也进入 dirty 更新，与对应 sparse 索引 在同一次上传中生效。空洞和 索引 复用不能让旧索引段引用另一个实例。
- 这三个 ID 都**不持久化**（回收后 generation 递增即可能变化），可保持私有；跨帧/跨存档的稳定引用一律用 `Guid`/`AssetID`。
- **不做显式 compaction**：有 active list 后通常无需压紧稳定 索引；真要消除长期空洞时，compaction 是显式非热路径操作，需重建 + 广播 remap。

Draw 由 GPU 剔除写入 indirect 列表；后续 GPU-driven geometry 复用相同 buffer layout 和 dirty update，不再重新定义场景输入。

### GPU-driven 边界

1. **instance 层已是一等公民**：transform 在 `RenderInstance`，索引关系为 `primitive → instance range → instanceIndices[] → RenderInstance`。当前每个 primitive 恰好一个 instance；多实例可以占用不连续索引，不改变 layout。
2. **layout 支持稀疏 dirty，上传策略可换**：当前排序并合并 range 后批量拷贝；GPU scatter 是 profile 后的优化，不影响场景 buffer。
3. **visibility 由 GPU 产出**：RenderPrimitive/RenderMeshBatch 不存 CPU visibility。`GpuDrawList`（Scene 模块）把 RenderScene 展开为 draw 数据并拥有 indirect 列表；列表通过 `DrawListFilter`（alpha mode、材质 tag、是否参与视锥剔除）注册，类似 SRP 的 renderer list。剔除 pass 留在 Renderer 中，一次 dispatch 填写全部列表，以后可替换为 HZB/实例级剔除而不改动绘制 pass。

## Editor contract

完整 Editor 尚未实现，但 World Foundation 必须满足：

- Inspector 可以通过 reflection 枚举和编辑组件属性。
- selection 和跨实体引用使用 `Guid`，不缓存 component pointer。
- 所有 mutation 都能表示为 create、destroy、add/remove component、set property 和 reparent 等 command。
- Edit World 与未来 Play World 可以独立存在。
- editor-only component 不进入 cooked World 和 RenderScene。

Editor 不应绕过 World API 直接修改 Flecs storage，否则无法可靠实现 undo/redo、prefab override 和 play-mode isolation。

## Threading and ownership

当前规定：

- 主线程拥有 World mutation、type registration 和 RenderScene extraction。
- ResourceLoader 与 asset decode 可以异步工作，通过 token 或完成队列交还结果。
- extraction 只在 World mutation 和 transform update 完成后执行。
- Renderer 每帧调用 `GpuScene::BeginFrame` 提取场景并写入 staging，`Upload` 在各 Pass 前记录拷贝，提交后调用 `EndFrame`。RenderScene 和 GPU 资产表在 GPU 上各只有一份常驻副本；图形队列按顺序执行，下一帧的拷贝排在上一帧的读取之后，因此不需要配对快照。StructuralChange 发布时，场景记录与资产表在同一次上传中更新。
- staging 按提交句柄轮换；全部仍在 GPU 使用时 `BeginFrame` 返回 false，不等待 fence。`BeginFrame` 与 `EndFrame` 之间不更新 RenderScene，Pass 直接读取 RenderScene 记录。
- dirty 索引 跨多次 update 累积，直到 `EndFrame` 确认上传；取消的帧保留这些变化。新建或扩容的表整表上传，资产 Ready 集合变化时资产表整表上传。
- GPU resource retirement 的所有权明确划分：Asset Manager 独占 Mesh/Texture/Material payload，引用归零后按提交退休；GpuScene 独占场景与资产表的 GPU Buffer 及 staging，替换或销毁时交给 `RenderContext::DeferRelease`。长期 asset lease 只阻止 资产管理项、当前 payload 和 GpuID 索引 的回收，不阻止历史版本清理。

详细设计见 [GPUScene](GPUScene.md)。

该模型与 `Multithreading.md` 的 phase boundary 方向保持一致：冻结的是 `BeginFrame` 到 `EndFrame` 之间的 RenderScene。后续并行 transform 或 extraction 时，不改变这些所有权边界。

关闭顺序固定为：停止新帧和 asset request；停止 World mutation/extraction 并销毁 World bindings，释放长期 leases；取消或 drain loader 工作；提交队列停止后等待 GPU 空闲，销毁 GpuScene；Asset Manager 在 RHI 仍存活时销毁 payload，RHI resource removal 同步释放其 descriptor；最后销毁 Renderer、queue 和 RHI device。任何阶段都不能让持有 GPU resource 的对象晚于 RHI device 析构。

## Testing

测试按层组织；以下 reload 指文件重新加载或离线重导入，在线 hot reload 尚未实现：

- Cooker tests：node hierarchy/local matrix round trip；Mesh/Material/内嵌 Texture 和 primitive 插入、重排后唯一匹配项保持 AssetID/SubmeshID；歧义项分配新 ID、旧引用保持未解析且不误绑；删除项不复用 ID；不匹配的格式和版本解析失败。
- Asset tests：重复请求、dependency cycle、每个加载阶段的故障注入、cancellation、迟到完成消息、loading 期间 release、长期 lease 随 MeshRenderer 删除而释放，以及三类 GpuID 的 stale handle。
- World tests：create/destroy、generation、Guid 冲突与查询、deferred 创建取消及删除后的身份清理、component query、deferred mutation、owning collection 在 component move/copy/remove/World exit 时的生命周期，以及 `DestroyEntity`（children reparent 到 root）与 `DestroySubtree`（整棵子树删除）的区别和显式层级删除策略。
- Transform tests：深层 hierarchy、cycle rejection、reparent、parent deletion、**奇异 parent 下 `SetParent(keepWorld)` 返回失败且 parent 不变**，以及 `LocalTransform`/`Matrix4` 互斥。
- Serialization tests：round trip、稳定输出、forward reference、非法 hierarchy、多版本 migration、reflected array resize/insert/remove、material override key 唯一且有序、同时带 `LocalTransform`+`Matrix4` 的非法文件，以及未知 component blob 保留。
- RenderScene tests：三层 索引 reuse、instanceIndices 正确引用不连续 索引、一个多-submesh Entity 展开成多个 RenderMeshBatch、asset-not-ready、entity removal 和无逐帧分配/逐 draw lease；常驻 GPU 表在多次 update、取消帧、删除和复用后与 RenderScene 一致，覆盖索引、active list 和 asset table。
- Render history tests：移动后下一次实际渲染推进 previous；只 Update 不 Draw、多次 Update 后提交、取消的帧都不提前推进历史；首次出现、teleport、camera cut 和窗口恢复正确重置历史。
- Live graphics tests：pending GPU work 下释放 World/Asset、严格 shutdown 顺序，以及多次 init/exit。

增加包含至少 100,000 个简单 Entity 的 CPU stress case，用于发现非预期的 O(N squared) 路径和逐实体分配；性能数字作为 telemetry 记录，不作为跨机器固定断言。

实现 hot reload 时补充 PayloadCompatible/StructuralChange 测试：场景记录与资产表在同一次上传中原子切换、依赖变化，以及 MeshRenderer 持续持有长期 lease 时反复重载，确认旧 payload 在其版本使用者和 GPU fence 结束后回收、当前 payload 与 GpuID 仍有效。

