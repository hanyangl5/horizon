# World Foundation

[TOC]

本文定义 Horizon 第一阶段 World Foundation 的目标和实施边界。最终交付是一个可验证的垂直切片：从稳定的 `AssetID` 加载 Bistro，在 ECS World 中创建实体和层级，保存并重新加载 World，提取 `RenderScene`，再由现有 Renderer 正确绘制。

World Foundation 是 GPU-driven geometry、light culling、path tracing 和 Editor 的共同数据基础。

## Current baseline

Horizon 已有以下基础：

- `SceneManager` 支持 cooked scene manifest、generation handle、异步资源状态，以及 geometry、texture 和 material buffer 的 GPU 生命周期。
- `AssetPipeline` 可以导入 glTF，并通过 cook fingerprint 判断输出是否过期。
- Flecs 已接入 Scene 的 World，实现实体、组件存储和缓存查询；公共接口不暴露 Flecs 类型。
- Renderer Example 直接消费 `SceneAssetInstance`，并在 CPU 上逐 instance 提交 draw。
- RenderContext 已支持 indirect draw，可在后续 GPU-driven pipeline 中复用。

当前 `SceneManager` 更接近 SceneAsset 加载器，而不是运行时 World。它不作为新架构的依赖；新系统独立定义资产目录、typed loaders、运行时状态和所有权。`SceneManager` 只保留为迁移期间的旧路径和测试行为参考，在 Renderer Example 完成迁移后移除。

## TODO: OpenUSD alignment

后续资产格式以标准 OpenUSD 为基准，Runtime 保留轻量组件，通过导入层映射 USD 语义。以下为待办，尚未实现；本文现有 Light 单位约定、glTF 导入方案和 `.world.json` 保存方案需据此复核。

- [ ] 明确 USD 是否同时承担场景保存，以及自定义 cooked 格式是否仅作为运行时缓存；确定后更新序列化与实施步骤。
- [ ] 优先修正 Light：按 UsdLux 核对 intensity、normalize、色温、形状坐标轴和默认值；独立表达 ShapingAPI，区分标准属性与引擎扩展。
- [ ] 扩展 Camera：投影类型、焦距、片门尺寸及偏移、光圈、对焦距离、快门时间；FOV 从镜头参数派生。
- [ ] 完善 Transform/Parent：支持 resetXformStack；导入层求值 xformOpOrder 和 pivot，保留无法分解的局部矩阵。
- [ ] 新增 Visibility/Purpose：处理继承可见性和 default/render/proxy/guide 用途。
- [ ] 完善 MeshRenderer 材质映射：解析整 mesh、面子集、继承和 collection 绑定；拓扑、primvars、细分参数归 MeshAsset，材质网络归 MaterialAsset。
- [ ] 增加实例表达：区分原生场景实例与 PointInstancer，支持 prototype 引用、实例 ID、变换数组和隐藏状态。
- [ ] 后续补充骨骼、蒙皮、BlendShape、动画与几何缓存。
- [ ] 在 Stage／导入层统一场景单位、坐标轴、颜色空间和时间基准；明确 USD prim 与 AssetID/ObjectID 的身份映射及回写边界。

## Goals

- 资产具有独立于路径和运行时 slot 的持久身份。
- Asset Registry 使用独立 typed loader pipeline，不依赖 `SceneManager` 的 manifest、固定容量或 callback 模型。
- Entity、持久化对象和 GPU instance 使用不同的 ID 与生命周期。
- World 使用 data-oriented ECS 存储组件，Flecs 类型不暴露在 Runtime 公共接口中。
- Transform hierarchy 支持动态创建、删除和 reparent，并只更新 dirty subtree。
- World 文件可确定性保存、加载和逐版本迁移。
- Reflection metadata 同时服务序列化、Editor Inspector 和未来 undo/redo。
- Renderer 只读取帧稳定的 `RenderScene`，不直接查询或持有 ECS storage。
- Asset 与 GPU 资源销毁遵守异步加载和 GPU fence 生命周期。
- Render extraction 热路径不进行内存分配。

## Non-goals

第一阶段不实现完整 Editor、prefab、scripting、network replication、自动多线程 ECS 调度、GPU culling、path tracing 或分布式 cooking。设计需要为这些功能保留稳定边界，但不提前实现其内部机制。

## Architecture

```text
AssetPipeline -> Cooked Asset
                       |
                 AssetRegistry
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
- `Scene/RenderScene` 管理渲染快照、sparse slot（+active list）和 GPU 更新范围。
- `ResourceLoader` 仍可承担底层 GPU upload，但其 token 和资源指针不成为 Asset Registry 的公共契约。

不新增平级 Runtime 模块。公共接口放在 `Source/Runtime/Scene/Public/Scene`，实现按 `Assets`、`World`、`Reflection`、`Serialization` 和 `RenderScene` 子目录放在 `Source/Runtime/Scene/Private`。Editor 后续作为独立 Tool target，只依赖 Scene 的公共接口。

第一方接口优先使用轻量 OOP：World、Asset Registry 和 RenderScene 由类封装状态与生命周期，资源所有权使用 RAII；ID 的生成和转换作为类型自身的方法。Component、metadata 和配置描述保留简单结构体，typed loader 继续使用函数表，不为统一接口引入继承或虚函数层次。

## Identity model

| Identity | Purpose | Lifetime |
| --- | --- | --- |
| `AssetID` | 标识 Mesh、Texture、Material、SceneAsset 等磁盘资产 | 跨路径移动、进程和 World 保存稳定 |
| `AssetHandle` | 定位 Asset Registry 中的一次运行时实例 | Registry slot 生命周期，包含 generation |
| `ObjectID` | World 文件、Editor selection 和跨实体引用 | 跨 World 保存稳定 |
| `Entity` | ECS 中的运行时实体 | 当前 World 生命周期，包含 generation |
| `RenderObjectID` | RenderScene 中一个 MeshRenderer 的 mesh 级记录（material overrides + instance range） | 不持久化；sparse slot + generation，回收后可变 |
| `RenderInstanceID` | 一个可渲染实例的 transform/bounds/identity | 不持久化；同上 |
| `RenderDrawID` | RenderObject × Submesh 的 draw 记录 | 不持久化；随 object/submesh 派生 |

`AssetID` 和 `ObjectID` 使用 128-bit 值，采用随机生成（UUIDv4 风格），不由内容或路径派生，因此重命名、移动或重新 cook 都不改变身份。碰撞概率可忽略，注册时仍校验唯一性并在冲突时报错而非静默覆盖。`AssetID` 由 importer 生成并写入 sidecar metadata，移动源文件时保持不变。`ObjectID` 在创建 authoring object 时生成。

测试和工具需要确定性时，通过显式注入的种子/ID 序列生成，而不是依赖全局随机源；这样 `.world.json` 的连续保存和回归测试可复现。

现有 `contentHash` 继续表示 source、import settings 和 cooker revision 的组合指纹。它用于判断是否需要重新 cook，不能作为资产身份。

运行时 handle 统一使用 index 加 generation。无效 handle 和 generation wrap 沿用 `SceneAssetHandle` 已有的**约定（pattern）**——仅指复用这套索引/代际语义，不构成对 `SceneManager` 代码的依赖。

## Asset Registry

Asset 系统分成三个部分：

- Asset Catalog 保存 `AssetID` 到 type、source URI、cooked URI 和 dependency IDs 的确定性映射。
- Asset Registry 保存运行时 slot、状态、引用计数、payload version 和错误信息。
- Typed Loader 负责特定资产类型的 IO、CPU decode、GPU upload、publish 和 destroy。

Catalog 可以在启动时从 cooked index 加载，也可以由 Editor 增量更新。Registry 不扫描磁盘来猜测资产身份。每个运行时 record 引用 Catalog entry，但不复制完整依赖和 manifest 数据。

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

- 重复请求同一 `AssetID` 时复用 record，不重复加载资源。
- dependency graph 在排队阶段检测循环依赖，父资产只有在必要依赖 ready 后才能进入 ready。
- request 和 release 不隐式等待 IO 或 GPU upload。
- loading 期间的最后一次 release 将 record 标记为 retiring；loader 完成后再安全清理。
- GPU payload 只能在相关 GPU 工作完成后销毁。
- 加载失败返回明确状态，并允许 renderer 使用 placeholder。
- Runtime 使用虚拟资源路径，不将绝对路径保存到 registry 或 World。
- Registry 更新集中在明确的 frame phase；后台线程只发布完成结果，不直接修改 World。
- 每个异步请求携带 slot generation 和 cancellation state；迟到的完成消息不能写入已经复用的 slot。
- CPU decode 失败、dependency 失败、GPU upload 失败和 cancellation 都必须进入同一清理路径。
- publish 是原子的；消费者只能看到旧 payload 或完整的新 payload，不能看到部分初始化状态。
- hot reload 创建新的 payload version。旧版本继续服务正在执行的帧，并在引用和 GPU fence 都完成后回收。
- shutdown 先停止接收请求，再取消或 drain 后台工作，最后等待并销毁已发布的 GPU payload。

状态机中 `WaitingDependencies` 与 `LoadingCPU` 的关系：依赖在进入 `Queued` 时即被**预留**（提高引用计数、触发其加载），本资产完成自身 CPU decode 后进入 `WaitingDependencies` 等待这些依赖 ready，再进入 `UploadingGPU`。即"预留在前、等待在后"，二者不冲突。

Typed Loader 使用显式注册的函数表，不依赖 RTTI。一个加载事务依次执行：解析 Catalog entry、预留依赖、后台 IO/CPU decode、提交 GPU upload、等待 upload token、发布 payload。PayloadCompatible 结果可以直接原子发布；StructuralChange 只提交 prepared candidate，由 frame-boundary transaction 与 RenderScene 更新一起发布。失败时按相反顺序清理已经取得的资源。Texture、Mesh、Material 和 SceneAsset 从第一版开始就是独立 loader；SceneAsset payload 只保存可实例化的 node/component 模板和其他 `AssetID`，不拥有其依赖资产的 GPU payload。

Asset Registry 可以在 loader 内部调用现有 ResourceLoader，但负责屏蔽其资源指针、callback 和同步细节。该边界允许未来替换 IO、加入 streaming 或接入 Editor asset daemon，而不改变 World 和 Renderer。

**GPU 资源所有权、稳定 GpuID 与间接层**：这里刻意与 RHI 的 descriptor 契约解耦——descriptor index 由 **RHI 在资源创建时分配**、随资源生命周期稳定（见 `Docs/Bindless.md`：texture 的 SRV/UAV、buffer 的 SRV、material 作为 table element、sampler 独立 heap，各自不同，**不存在单一的"资产 bindless index"**）。因此分层如下：

```text
AssetID
  → Asset payload（typed loader 拥有）
    → GpuMeshRecord / GpuMaterialRecord / GPUTexture
      → resource-specific descriptor indices（由 RHI 分配/回收）
```

- Asset Registry 只拥有 **typed payload 生命周期**，不自己管 descriptor 分配；RHI 继续拥有 descriptor allocator。
- Registry 维护版本化的 GPU 间接表，例如 `GpuMeshTable[GpuMeshID] → 当前 mesh payload（含各 submesh 的 index/vertex descriptor）`；Material 和 Texture 使用各自的 `GpuMaterialTable` 与 `GpuTextureTable`。这些 GpuID 不是裸 descriptor index。
- `GpuSubmeshRef` 是运行时组合引用 `{GpuMeshID, submeshIndex}`，不是独立 ID。它只在创建它的 mesh layout/table snapshot 内有效；发生 structural reload 后必须重新解析稳定 `SubmeshID`，不能把旧 `submeshIndex` 当成持久身份。
- RenderDraw 保存 `GpuSubmeshRef`/`GpuMaterialID`，不保存 descriptor index、`SceneManager` slot 或裸指针。resolve 时先由 `GpuMeshID` 得到当前 snapshot 内的 mesh payload，再以 `submeshIndex` 取得 submesh descriptor。
- **hot reload 分两类，不能一概而论"RenderScene 无 dirty"**：
  - **PayloadCompatible**（layout 与 bounds 不变，只替换纹理像素、材质常量等）——只更新间接表里该 ID 指向的 payload version，RenderScene 不用 dirty。旧 payload 退休后，在其 snapshot/版本使用者全部释放且相关 GPU fence 完成时销毁，不等待长期 asset lease 清零。
  - **StructuralChange**（submesh 数量/稳定映射、bounds、默认材质、vertex/index layout 改变）——Registry 产生失效信息，resolve/binding 层根据稳定 `SubmeshID` 重建受影响的 RenderDraw 和 bounds。这类 reload 无法对 RenderScene 透明。
  - **texture reload** 要求 Material table 存的是 `GpuTextureID`（经间接表解析），不是旧 texture descriptor；否则父 Material 不 reload 就会引用已退休的 descriptor。
- StructuralChange 的 publish 是一个 frame-boundary transaction：先准备新 payload，根据稳定 `SubmeshID` 重建 binding、RenderDraw 和 bounds，再构造匹配的 RenderScene/GPU table snapshot，最后一起 publish。旧帧继续使用相互匹配的旧 RenderScene、旧 table 和旧 payload，禁止出现“新 table + 旧 RenderDraw”的中间状态。
- `GpuMeshID`、`GpuMaterialID` 和 `GpuTextureID` 都是运行时 `{index, generation}` handle，在对应 Registry record 驻留期间跨 payload reload 稳定。只有长期 asset leases、所有 snapshot 引用和相关 GPU fences 都结束后才能回收 table slot；复用时递增 generation。CPU 在构造 snapshot 时验证 generation，GPU 只消费该不可变 snapshot 中已经验证过的 table index，因此旧 ID 不会在执行中的帧内别名到另一份资产。

**三层引用生命周期（必须区分，否则活着的 MeshRenderer 引用的资产会被卸载）**：

1. **SceneAsset 实例化期的 dependency lease**：只在"加载 SceneAsset → spawn entities"这段事务里持有，是**瞬态**的。SceneAsset record 一旦被释放（它的用途只是产出 entity），这些 lease 就释放——**它不能作为运行期常驻引用的持有者**。
2. **运行期长期 lease（本文新增的关键持有者）**：由 Scene 的 **resolve/binding 层**为**每个活跃的 `MeshRenderer`** 持有一份对已解析 Mesh/Material 的长期 asset lease；component 删除（或 Entity 销毁）时释放。这才是"World 里 MeshRenderer 存活 ⇒ 资产常驻"的保证。因此 `MeshRenderer` **只存 `AssetID`（不存 `AssetHandle`）**——handle 是 Registry slot 生命周期，绝不能泄漏进 ECS 和序列化。
3. **单帧 snapshot lease**：每帧只取得一次配对的 RenderScene/GPU asset table snapshot lease，由 snapshot 固定其引用的 payload versions；不按 RenderDraw 或 instance 增减引用计数。提交 fence 完成后释放 snapshot（见 Threading）。

长期 asset lease 保活的是 **record 和当前 payload**，不固定历史 payload version。当前版本通过 record 持有依赖 lease；reload candidate 先取得自身所需依赖，发布后旧版本所需的依赖仍保留到该版本退休清理完成。旧版本的回收只等待其 snapshot/版本使用者和相关 GPU fence；record 与 GpuID slot 的回收才要求长期 lease 清零、后台工作结束且所有版本完成清理。Payload 只保存 `AssetID` 或已解析的稳定 GpuID，不保存依赖 GPU 指针。依赖的 hot reload **不**自动重载父资产，除非父 loader 显式声明"依赖版本会影响自身 payload"。

Asset metadata 初期使用 sidecar 文件保存 `AssetID`、type、source URI 和 cooked URI。Cooker 输出继续保存 content hash 和依赖信息。

## SceneAsset 实例化

SceneAsset 与 World 的粒度必须先定死，否则垂直切片无法落地。第一阶段采用 **node → Entity** 映射：

- SceneAsset payload 保存一棵 **node 模板树**：每个 node 含**局部变换（tagged：`LocalTransform` 或 `LocalMatrix` 二选一——glTF node 本身可用 matrix 形式，不可分解时保留为 matrix）**、parent 索引，以及（对有几何的 node）一个 `MeshRenderer` 模板。它不保存烘焙后的世界矩阵，也不拥有依赖资产的 GPU payload。
- **一个 node 对应一个 Entity，而不是一个 primitive 一个 Entity。** glTF 一个 mesh node 可含多个 primitive、每个 primitive 有独立材质（现有 cooker 正是逐 primitive 生成实例，见 `AssetPipeline.cpp:1955`）。因此这些 submesh 归到一个 `MeshAsset` 里，由**一个** node Entity 的 `MeshRenderer` 引用；不为每个 primitive 建独立 World Entity——除非该 primitive 确需独立 transform 或编辑身份。
- `MeshRenderer` 结构：`mesh: AssetID` + **material override set（稀疏）**。`MeshAsset` 内保存 submesh/primitive 表和每个 submesh 的**默认 material 绑定**；`MeshRenderer` 只记录与默认不同的槽（天然对接未来 prefab override）。读取时 default + override 合并。
- 实例化时为每个 node 创建 Entity，重建局部变换 + `Parent` 层级，并在其上挂一个 instance root Entity，使整份 Bistro 可作为一个单位被移动或 reparent；**删除用 `destroySubtree(instanceRoot)`**（见 Transform hierarchy），保证整棵子树一起删、不留孤儿节点。
- 同一 SceneAsset 多次实例化时 node 各自独立成 Entity，但引用的 Mesh/Material 通过 `AssetID` 复用同一份依赖 record（多个 Entity 共享这些资产 record）。
- **子资源身份分两类**：submesh 使用 `MeshAsset` 内部的稳定 `SubmeshID`（不是全局 `AssetID`）；导出的 Mesh、Material 和内嵌 Texture 都是独立资产，各有 `AssetID`。源文件 sidecar 保存 SceneAsset ID 和完整子资产身份表，包括 type、持久 ID、source 匹配信息，以及每个 Mesh 的 SubmeshID 表。引用已独立注册的外部 Texture 时复用其 AssetID。
- **重导入统一匹配规则**：首次导入分配并持久化上述 ID；重新导入优先使用 exporter/source 提供的持久身份，名称、几何和材质签名仅用于辅助唯一匹配，数组索引和内容 hash 不能作为身份。插入、重排或重命名后，能唯一匹配的资源沿用 ID；歧义项分配新 ID 并报告。被删除或无法匹配的旧条目保留为 missing，不将其 ID 分配给其他资源；已有 World 引用或 material override 保持原 ID 并报告未解析，不能静默改绑。Cooked 引用与 sidecar 身份表必须一致后才能发布 Catalog。

第一阶段先版本化 cooked SceneAsset schema，并让 cooker 输出 node 层级、局部变换、MeshAsset 引用及其 submesh/default-material 表，而不是把现有 `SceneAssetInstance` 那种“每 primitive 一个烘焙 `world[16]`”的扁平数组作为正式模型。World loader 遇到只有旧 flat 数据的 schema 时返回明确的 re-cook 错误，不为它扩展 `MeshRenderer`。迁移期间 cooker 可以在新版资产中额外双写 legacy compatibility chunk 供旧 Renderer 使用；新 World 路径忽略该 chunk，Renderer Example 完成迁移后再移除它。

World 内部使用 Flecs。公共 `Entity` 是轻量的 64-bit value，Flecs world、query 和 component ID 只存在于 `World/Private`，避免第三方接口扩散到 Runtime、Editor 和 Renderer。

第一批组件为：

- `ObjectIdentity`
- `Name`
- `LocalTransform`（TRS）
- `LocalMatrix`（可选，见 Transform hierarchy；import 用它承载不可分解的仿射变换）
- `Parent`（公共 API 视图，内部由 Flecs `ChildOf` 承载，非独立真相来源）
- `WorldTransform`（派生数据，不进 World 文件，存于层级有序存储）
- `MeshRenderer`
- `Light`
- `Camera`

Component 保存 authoring 或 simulation 数据，不拥有 GPU resource。`MeshRenderer` **只保存 `mesh: AssetID` 和 material override set**——不保存 `AssetHandle`、`Geometry*`、`GPUBuffer*`、descriptor index、`GpuMeshID` 或 RenderScene slot（这些都是运行期状态，由 resolve 层维护并持有长期 lease，见 Asset Registry 一节）。

System 管理行为，例如 transform propagation 和 render extraction。第一阶段采用确定性的单线程更新顺序；query 迭代期间的结构变化进入 deferred command buffer，并在 phase boundary 应用。多线程调度在 World contract 和单线程结果稳定后再接入。

当前 `World`、`WorldQuery` 和 `DeferredChanges` 使用 RAII，内存接入 tf allocator。World 在构造时注册 TypeRegistry 中已有的类型，metadata 必须比 World 活得更久。查询只提供组件的只读视图，缓存查询应复用；结构修改在最外层 defer 作用域结束时应用。沿用 Flecs 的语义：`setComponent` 对已有组件立即更新值，对尚不存在的组件保存独立副本并延后添加，因此 defer 不提供值的快照隔离。需要覆盖整个更新阶段时，由调用方持有外层 defer 作用域。

### Flecs 复用边界

Flecs 自带 meta（反射）、JSON 序列化、`ChildOf` 关系和 deferred 操作。为避免重复造轮子又不让第三方类型泄漏，划清复用 / 自建边界：

- **复用 Flecs**：component storage、query、`ChildOf` 关系作为 `Parent` 的内部存储、以及 deferred command queue。这些是 Flecs 的强项，且不进入公共接口。
- **自建（不使用 Flecs meta / JSON）**：反射 type/property registry、`.world.json` 序列化、component migration。原因是本项目对这三者有 Flecs meta/JSON 不满足的硬需求——**确定性输出**（稳定排序 + 固定 float 格式，用于 diff 和回归）、**逐版本 schema migration**、**同一份 metadata 复用于 Editor Inspector 与未来 undo/redo command**，以及**不把 Flecs 类型暴露到 Editor/Renderer**。用 Flecs JSON 会把其数据模型和格式细节固化进 World 文件，迁移和 Editor 掌控都受限。
- **不自建**的部分（如 hierarchy 存储）直接用 `ChildOf`，只在 Public 层用不透明 `Parent`/`Entity` 封装。

该边界让"隐藏 Flecs 类型"这个目标不等价于"重造 ECS"，把自建工作量收敛到确实需要自控的三块。

## Light authoring

`Light` 按离线渲染常用发光形状组织，类型划分参考 [UsdLux](https://openusd.org/dev/user_guides/schemas/usdLux/overview.html)，面积发射参考 [PBRT](https://www.pbr-book.org/4ed/Light_Sources/Area_Lights)。当前只实现组件与反射；尚未接入灯光 extraction、采样或着色，也不是完整的 UsdLux schema 映射。

| 类型 | 几何与方向 | intensity 的本地约定 |
| --- | --- | --- |
| Directional | 沿局部 -Z；angularDiameter 为完整角直径，0 为理想平行光 | 垂直于主方向平面的辐照度，W/m²；改变角直径时保持该值 |
| Point / Spot | 点光源；Spot 沿 -Z，内外锥角为半角 | 辐射强度，W/sr；Spot 为轴上值 |
| Rect / Disk | 局部 XY 平面，朝 -Z；宽高或半径 | 辐亮度，W/(m²·sr) |
| Sphere / Cylinder | 球心位于原点；圆柱沿 Y，仅侧面发光 | 辐亮度，W/(m²·sr) |
| Dome | 无限远环境，使用实体旋转；平移及缩放不影响它 | 辐亮度，W/(m²·sr) |
| Mesh | 同一实体的 MeshRenderer 提供全部三角形及 UV，朝向使用几何法线 | 辐亮度，W/(m²·sr) |

几何尺寸为局部米制长度，世界变换决定最终发光表面；所有角度使用弧度。颜色使用场景线性 RGB，曝光乘数为 `2^exposure`；开启色温时，以 Kelvin 定义的黑体颜色乘入 color。正尺寸约束、内锥不大于外锥等输入校验将在属性编辑和加载路径接入；reflection 的范围目前只是编辑提示。

面积光默认单面 Lambertian 发射；`twoSided` 开启双面发射。`normalize` 只作用于面积光：intensity 改为纹理调制前的总功率 W，基准辐亮度为 `intensity * 2^exposure / (π * A)`，A 为世界空间发光面积，双面时计入两面。颜色和纹理继续调制该基准值，不隐式补偿其平均亮度。非面积光忽略 normalize 和 twoSided。

`texture` 是可选发光贴图 AssetID：Dome 使用经纬图，Rect/Disk 使用局部平面 UV，Sphere/Cylinder 使用经纬/侧面 UV，Mesh 使用原始 UV；无贴图为白色，Point/Spot/Directional 忽略此字段。纹理解释为场景线性值。Mesh Light 的显式发射替代该表面的材质自发光，后续提取不能将两者重复计入。`range = 0` 表示不截断；正值仅作为有限光源的渲染近似，Dome/Directional 忽略它。

## Transform hierarchy

**Local 变换有两种表示，二者严格互斥**：`LocalTransform`（TRS：translation/rotation/scale）是 authoring 首选；`LocalMatrix`（可选组件）承载**无法无损分解为 TRS** 的仿射变换。原因：父级非均匀缩放与旋转组合后，world/local 矩阵可能含 **shear**，TRS 无法精确表达；负缩放也有歧义。**互斥由 World API 强制**——一个 Entity 二者只能有其一；加载到同时带两者的**非法文件直接报错**（不做"谁优先"的隐式规则）。importer 对不可分解的局部变换使用 `LocalMatrix`，作者手编或可分解的节点使用 `LocalTransform`。`Parent` 表示层级关系；`WorldTransform` 保存 current 和 previous matrices，属于派生数据，不写入 World 文件。

Hierarchy 必须满足：

- `setParent` 拒绝 self-parent 和 cycle。
- `setParent(..., keepWorld=true)` 尝试解 `newParentWorld · local = childWorld`。第一阶段不实现奇异矩阵的广义求解：只要 new parent world 不可逆就返回失败并保持原 parent。
  - **可解**：结果可分解则写 `LocalTransform`，否则写 `LocalMatrix`（保留精确仿射结果，绝不有损 TRS 近似）。
- `setParent(..., keepWorld=false)`：正常 reparent，直接沿用现有 local，不受上面限制。
- **两种删除操作，语义必须分开**：`destroyEntity(e)` 删单个 Entity，其 children **reparent 到 root 并保持 world transform**（root=单位阵、恒可逆，keepWorld 恒可解，必要时落 `LocalMatrix`）；`destroySubtree(e)` 删 e 及**整棵 transform subtree**。**SceneAsset instance 的删除用 `destroySubtree`**（删 instance root = 删整份 Bistro），否则会把节点遗留在 World。
- **Flecs `ChildOf` 删除策略必须显式设定**：Flecs 默认 `(OnDeleteTarget, Delete)`（删父连带级联删子），与 `destroyEntity` 的"children reparent 到 root"相反。World API 要显式控制该关系策略，让 `destroyEntity`/`destroySubtree` 的语义盖过 Flecs 自动 cascade，避免二者混用产生歧义或漏删/误删。
- local 或 parent 变化只标记受影响 subtree。
- 更新使用迭代式 root-to-leaf 顺序，不依赖递归调用栈。
- teleport 或 camera cut 可以显式同步 previous transform，避免错误 motion vector。
- Transform update 在 RenderScene extraction 前完成。

**单一真相 + 派生遍历 cache**：层级的**唯一权威来源是 Flecs `ChildOf`**（`Parent` 只是它的公共视图，不是第二份存储）。Flecs 按 archetype/table 存储，普通 query 迭代**不是**层级顺序，也无法便宜地"只遍历 dirty subtree"。因此从 `ChildOf` **派生**一个层级遍历 cache——parent 恒在 child 之前的拓扑序（DFS）数组，带一个 `structureVersion`。propagation 沿该数组从前往后单次线性扫描；dirty 用 dirty-root 队列表示，重建后每个 subtree 是数组里一段**连续 DFS range**，clean subtree 直接跳过。`WorldTransform`（current/previous）作为派生数据存在这套 dense、hierarchy-ordered 存储里，而不是靠普通 component query 逐个更新；它不进 World 文件。

**结构变化（create/delete/reparent）时全量 O(N) 重建该 cache 并递增 `structureVersion`**，不追求增量拓扑维护。理由：要同时保住"parent 在前 + subtree 连续可跳过 + reparent 不移动数据"三者，数学上做不到（reparent 必然打乱 DFS 连续性）；而结构变化通常远少于 transform 更新，第一版用重建换正确性和简单性，增量维护留到有 profile 证据再做。

后续并行化时以独立 root 或预计算的 hierarchy range 为任务单位，结果必须与单线程路径一致。

## Reflection and property metadata

Reflection 使用显式 type registry，不依赖 C++ RTTI、异常或复杂模板实例化。组件在 Runtime 初始化期间显式注册，不能依赖跨模块静态初始化顺序。

每个 component descriptor 包含稳定 type ID、名称、schema version、size、alignment、property span 和可选 migration callback。含 owning collection 的 component 还必须注册显式 init/copy/move/destroy lifecycle operations，并由 World 转接为内部 Flecs hooks，保证 archetype 移动、deferred command 和 World 销毁不会浅拷贝或泄漏 collection storage。每个 property descriptor 包含稳定 property ID、名称、类型、offset、size、flags，以及 Inspector 所需的 range、step 或 widget hint；集合 property 还要引用 element descriptor 和统一的 collection operations。

第一版支持以下 property 类型：

- boolean、signed/unsigned integer 和 float
- vector、quaternion 和 color
- bounded string 和 enum
- `AssetID` 与 `ObjectID` reference
- **array-of-struct**：元素是固定 schema、可平凡复制的小结构体，字段仍由上述标量/ID property 描述。第一阶段复用现有 `hz::Array` 及 tf memory，不新增 Scene 容器或 allocator；metadata 提供 element descriptor 和统一的 `ArrayOperations`，完成 count/data 查询及 resize/insert/remove，消费者不依赖数组内存布局。Serializer、Inspector 和 undo/redo 不直接缓存 data pointer，所有 mutation 都经过 World property API，以便产生 dirty/change notification。

`MeshRenderer` 的 override array 元素为 `MaterialOverride { SubmeshID, materialAssetID }`。World mutation API 保持该数组按 `SubmeshID` 排序且 key 唯一；loader 在 staging World 中验证并规范化输入，serializer 直接按存储顺序输出，不在保存热路径复制或排序数组。

Property flags 至少区分 serialized、transient、read-only 和 editor-only。注册时检查重复 ID、名称冲突、越界 offset、不完整的 array element metadata 和不支持的字段类型。

同一份 metadata 服务以下消费者：

- World serializer
- Editor Inspector
- 未来 command/undo/redo
- prefab override
- property animation 和 remote control

第一阶段手工声明少量 descriptor；类型规模增加后再引入代码生成。不能为每个消费者维护一套独立字段表。

## World serialization and migration

Cooker 当前已经使用 `.scene.json`。为避免 authoring scene 与 cooked SceneAsset 混淆，第一阶段使用 `.world.json` 作为可编辑 World 格式。

World 文件保存：

- file schema version 和 World ID
- 按 `ObjectID` 稳定排序的 entity records
- entity name 和 parent `ObjectID`
- component name、component version 和 serialized properties
- `AssetID` reference，不保存 runtime handle、pointer 或 GPU index

加载使用分阶段提交：

1. 验证顶层 schema，并创建 staging World。
2. 创建全部 Entity，建立 `ObjectID` 映射。
3. 添加和迁移 Components。
4. 解析 parent 与其他对象引用。
5. 验证 hierarchy 并更新 transforms。
6. 全部成功后原子替换目标 World。

任何失败都不能留下部分修改的目标 World。输出顺序和 float formatting 必须确定，使相同 World 连续保存得到相同内容。

Component 独立维护 schema version，migration 按相邻版本顺序执行，例如 `v1 -> v2 -> v3`。纯 Runtime 加载遇到未知 component 或无法迁移的版本时返回明确错误。但**文件格式从第一版就必须允许保留未知 component 的原始 JSON blob**：否则 Editor 打开由更新版本或新插件创建的 World 会无法恢复、round-trip 丢数据。保留原始 blob 是格式层的硬要求；Editor 侧"读取-透传-回写"未知数据的完整实现可延后，但格式不能事后再加。

Cooked World 将来可以使用分块二进制格式，但不能直接 dump ECS 内存布局。

## RenderScene extraction

RenderScene 是 World 与 Renderer 的唯一数据边界。它属于 Scene 模块，负责保存当前可渲染世界的帧稳定表示；提取发生在 update phase 完成之后，Renderer 在该帧不再访问 ECS。

**三层模型（一个 MeshRenderer Entity 会展开成多个 draw）**：一个 node Entity 的 mesh 含多个 submesh、各有材质，因此"一 instance = 一个 GpuMeshID/GpuMaterialID"不成立。RenderScene 分三层（术语上区分 glTF **submesh/primitive** 与 UE 意义的 scene **RenderObject**）：

```text
MeshAsset
  └─ Submesh records（共享静态几何；GpuSubmeshRef = {GpuMeshID, submeshIndex}）

RenderObject (RenderObjectID)  = 一个 MeshRenderer Entity
  ├─ resolved material overrides（default + override 合并后的结果）
  └─ instance range → instanceIndices[] → RenderInstance sparse slots

RenderInstance (RenderInstanceID)
  └─ current/previous world transform + world bounds + ObjectID(picking)

RenderDraw (RenderDrawID)  = RenderObject × Submesh
  └─ RenderObjectID + GpuSubmeshRef + GpuMaterialID
```

- **transform 从第一天就在 per-instance 层（`RenderInstance`），不在 `RenderObject` 上**——这样将来一个 object 挂多 instance（ISM/foliage）不改 buffer layout。
- **RenderObject** 持 mesh 级共享数据（material overrides、instance range），一个 Entity 一份。Range 的 offset/count 指向 `instanceIndices`，不是 RenderInstance sparse slot 的连续范围；索引元素指向 snapshot 内已验证 generation 的 instance slot。实例增删可以迁移索引段并更新 object range，不移动存活的 instance slot；旧帧继续使用自身 snapshot 的 range 和索引段。
- **RenderInstance** 持 transform/bounds/identity。**phase 1 每个 RenderObject 恰好 1 个 RenderInstance**（一 Entity → 1 object + 1 instance + N 个 RenderDraw，N=submesh 数）。
- **RenderDraw** 是 draw 模板（object × submesh），持 snapshot 内有效的 `GpuSubmeshRef` 和 resolve 后的 `GpuMaterialID`（descriptor 走 Asset Registry 间接表，不存 descriptor index）；draw 时按 object 的 instance range 展开。
- lights、cameras 同为 RenderScene 顶层数组。
- 可见性**不**作为 CPU mask 存储——见下方 GPU-driven 预留；未来 GPU culling 输出的正是可见 (RenderInstance × RenderDraw)（或 meshlet）列表。

主要约束：

- Entity 获得稳定的 RenderScene slot，直到对应 render component 被移除。
- extraction 输出 created、updated 和 removed slots，并合并连续 dirty ranges。
- 数据数组提前 reserve；正常帧的 extraction 和 GPU upload 不分配内存。
- asset 未 ready 时使用 placeholder 或跳过 instance，不阻塞渲染线程。
- 删除 Entity 时，CPU slot 可以立即失效，但其 GPU backing storage 必须按 frames-in-flight 延迟回收。
- **渲染历史按实际提交推进**：hierarchy update 只更新 current；extraction 的 previous 取上一次已提交渲染快照的 current，不取上一次 Update 或 extraction 的结果。提交成功后才推进历史基准，第一版使用 history-dirty 列表，使下一次渲染即使对象未移动也上传 previous=current；未提交的快照不消耗历史。首次出现、teleport、camera cut 和窗口恢复渲染后的首帧使用 previous=current；恢复时同步重置相机历史。只有 Update、连续多次 Update 或丢弃 extraction 都不推进渲染历史，也不修改已提交快照。
- RenderScene 不包含 Flecs 类型、authoring 路径或 Asset Registry 内部 pointer。

**slot 模型（稳定 ≠ 密集，用 sparse + active list 两层）**——`RenderObjectID`、`RenderInstanceID`、`RenderDrawID` 各自的数组套用同一模式：
- **stable sparse slot array**：id 指向一个稳定 slot（带 generation），对应 render 数据存活期间**不移动**；删除时回收进 **free-list** 供复用。因为有 free-list，数组是**稀疏的（有洞）**，不是 densely packed——Renderer 不能盲目遍历 `[0, capacity)`。
- **dense active-index list**：每层另存一个当前活跃 slot 的紧凑列表。CPU 遍历、以及未来 GPU culling，都遍历 active list，而不是稀疏数组。
- **dirty upload**：按被创建/更新的 sparse slot 的 index range 上传（稀疏 dirty 见下方 scatter 预留）。
- `instanceIndices`、object range 和 active list 的修改、计数变化也进入 dirty 更新，与对应 sparse slot 在同一 snapshot 内生效。空洞和 slot 复用不能让旧索引段引用另一个实例。
- 这三个 ID 都**不持久化**（回收后 generation 递增即可能变化），可保持私有；跨帧/跨存档的稳定引用一律用 `ObjectID`/`AssetID`。
- **第一阶段不做显式 compaction**：有 active list 后通常无需压紧稳定 slot；真要消除长期空洞时，compaction 是显式非热路径操作，需重建 + 广播 remap。

第一阶段仍允许 CPU 遍历 RenderScene 提交 draw。下一阶段的 GPU-driven geometry 直接复用相同 buffer layout 和 dirty update，不再重新定义场景输入。

### GPU-driven 预留（contract，不在 phase 1 实现）

为使"phase 2 复用同一 layout"真正成立，layout 从一开始就预留以下三点（仅约定，phase 1 用 CPU 路径填充，不实现 GPU 逻辑）：

1. **instance 层已是一等公民**：三层模型里 transform 就在 `RenderInstance`。Phase 1 每 object 恰好 1 instance，但索引关系从第一版就是 `object → instance range → instanceIndices[] → RenderInstance sparse slots`；未来多实例可以占用不连续 slot，无需改变这层索引布局。实例分组、分配和 culling 策略留到后续实现。
2. **layout 支持稀疏 dirty；上传策略可换**：buffer layout（sparse slot）必须能表达稀疏 dirty 集，这是契约；但**具体上传方式不是 layout 契约**——phase 1 用"排序 + 合并 range"上传即可，GPU scatter 是 profile 后的优化，二者可互换、不影响场景 buffer。
3. **visibility 由 GPU 产出**：不在 RenderObject/RenderDraw 上存 CPU visibility mask。约定一条独立的**可见 RenderDraw 列表**（+ indirect draw args），phase 1 由 CPU 直接写这条列表，phase 2 改为 GPU culling（frustum/HZB occlusion）产出同一条列表，CPU/GPU 路径共用同一场景输入。

## Editor contract

完整 Editor 不属于第一阶段，但 World Foundation 必须满足：

- Inspector 可以通过 reflection 枚举和编辑组件属性。
- selection 和跨对象引用使用 `ObjectID`，不缓存 component pointer。
- 所有 mutation 都能表示为 create、destroy、add/remove component、set property 和 reparent 等 command。
- Edit World 与未来 Play World 可以独立存在。
- editor-only component 不进入 cooked World 和 RenderScene。

Editor 不应绕过 World API 直接修改 Flecs storage，否则无法可靠实现 undo/redo、prefab override 和 play-mode isolation。

## Threading and ownership

第一阶段规定：

- 主线程拥有 World mutation、type registration 和 RenderScene extraction。
- ResourceLoader 与 asset decode 可以异步工作，通过 token 或完成队列交还结果。
- extraction 只在 World mutation 和 transform update 完成后执行。
- Renderer 每帧消费一个不可变 `SceneFrameSnapshot`，其中配对保存 RenderScene snapshot 与 `GpuAssetTableSnapshot`。StructuralChange 只在能同时发布这两部分时提交。
- Snapshot 从预分配的 frames-in-flight pool 取得；一帧只 acquire/release 一次 snapshot lease，不为每个 RenderDraw、instance 或资产执行引用计数，也不在 extraction 热路径构造动态 lease 集合。提交后把 fence value 记录到 snapshot，fence 完成后才允许复用该 slot 和回收其 payload versions。
- **每份 backing storage 独立补齐增量**：配对的场景状态使用递增 revision，pool 中每份 RenderScene/GPU table backing storage 记录 `appliedRevision`。Fence 完成后复用时，合并从该版本到目标 revision 的全部变化，再从目标状态写入；不能只上传本帧 dirty。变化覆盖创建、更新、删除、历史变换推进、slot 复用、active list、instanceIndices、计数和 asset table。两侧都同步到目标 revision 后才能发布配对快照。
- 增量记录使用预分配的有界 journal，保存变更位置而不持有旧 payload 指针；新 backing storage 或版本落后于 journal 保留范围时完整同步。正常帧补齐累计 dirty ranges，首次使用、扩容或 journal 溢出允许完整上传。同步完成前不能重新 acquire 该快照，也不能消费其中已退休的旧 descriptor；fence 完成并释放 lease 后，残留的 backing 内容本身不保活 payload。
- GPU resource retirement 的所有权明确划分：Asset Registry 独占 Mesh/Texture/Material payload 和 GPU table snapshots；RenderScene 独占自身 object/instance/draw/light/camera buffers、instanceIndices 和 snapshots。`SceneFrameSnapshot` 只持有两侧 snapshot leases，不取得底层资源所有权。退休 payload 按版本使用者和 GPU fence 回收；长期 asset lease 只阻止 record、当前 payload 和 GpuID slot 的回收，不阻止历史版本清理。

该模型与 `Multithreading.md` 的 frame snapshot 和 phase boundary 方向保持一致。后续并行 transform 或 extraction 时，不改变这些所有权边界。

关闭顺序固定为：停止新帧和 asset request；停止 World mutation/extraction 并销毁 World bindings，释放长期 leases；取消或 drain loader 工作；提交队列停止后等待所有 snapshot fences，释放 snapshots；Asset Registry 在 RHI 仍存活时销毁 payload，RHI resource removal 同步释放其 descriptor；最后销毁 Renderer、queue 和 RHI device。任何阶段都不能让持有 GPU resource 的对象晚于 RHI device 析构。

## Staged rollout

当前已完成第 1～3 步：独立 ID、显式 reflection、7 个内置组件，以及 World 的 generation Entity、组件增删读写、缓存查询和嵌套 defer。当前创建的是运行时实体；authoring ObjectID 自动分配及查找尚未接入。Parent/WorldTransform 和局部变换互斥随第 4 步实现；属性级编辑、override 排序约束和序列化迁移尚未接入。

1. 增加 `AssetID`、`ObjectID`、格式化、解析、hash 和相等比较测试。
2. 增加显式 type/property registry，并注册第一批内置组件。
3. 接入 Flecs，提供 World、generation Entity 和 deferred mutation 生命周期。
4. 实现 transform hierarchy、dirty propagation 和 previous transform。
5. 版本化 cooked SceneAsset schema，并更新 AssetPipeline 输出 node tree、局部变换、MeshAsset/submesh/default-material 表；sidecar 保存完整子资产 AssetID/SubmeshID 身份表并支持重导入匹配。旧 schema 在新 World 路径中明确要求 re-cook，迁移期按需双写仅供旧 Renderer 使用的 compatibility chunk。
6. 实现独立 Asset Catalog、Asset Registry 和 Texture、Mesh、Material、SceneAsset typed loaders，完成请求去重、dependency transaction、cancellation 和 retirement。
7. 实现确定性的 `.world.json` 保存、加载和 component migration。
8. 实现 RenderScene 三层 slot（RenderObject/RenderInstance/RenderDraw）、instanceIndices 和 resolve/binding 层（解析资产引用并持有长期 lease），完成按提交推进的历史、各 backing storage 的 revision 增量补齐、配对 snapshot publish 和 frame-safe retirement。
9. 将 Renderer Example 从直接消费 `SceneAssetInstance` 迁移为消费 RenderScene。
10. 完成 Bistro load、instantiate、edit、save、reload、render 和 shutdown 集成测试。

**关键路径与可延后项**：上面的切片关键路径是 1→2→3→4→5→6→7→8→9→10，即“cook node hierarchy → 加载 Bistro → 建实体/层级 → 存 → 重载 → 渲染”。但要区分安全必需与纯增强——前者不做会导致挂起或 use-after-free，必须在第一阶段实现：

- **第一阶段必须**：
  - **依赖环检测**——否则父资产可能永久 stuck 在 `WaitingDependencies`。
  - **request generation / 迟到完成消息防护**——迟到的完成结果不能写进已复用的 slot。
  - **逻辑 cancellation**——release 后不要求真的中止底层 IO，但其完成结果**只能进清理路径**，绝不 publish 到已释放的 record。
- **可延后到 phase-1.5**：
  - **主动中止底层 IO/decode**（逻辑取消已经保证正确，主动中止只是省资源）。
  - **hot reload 完整实现及专项测试**（间接表、version 和退休版本生命周期的 contract 现在就留；触发重载、PayloadCompatible/StructuralChange 切换及验证延后。第一阶段仍实现初次加载、卸载与逐帧配对快照的一致性）。

step 6 与 step 8 是长线工程，建议进一步拆成可独立合入的小步。

每一步保持为可独立测试和 review 的修改，不同时进行大规模命名迁移或重写 AssetPipeline。迁移期间允许旧 `SceneManager` 路径继续存在，但新模块不得 include、调用或包装它；Renderer Example 达到功能对等后再单独删除旧路径。

## Testing

第一阶段测试按层组织；以下 reload 指文件重新加载或离线重导入，在线 hot reload 测试归 phase-1.5：

- Cooker tests：node hierarchy/local matrix round trip；Mesh/Material/内嵌 Texture 和 primitive 插入、重排后唯一匹配项保持 AssetID/SubmeshID；歧义项分配新 ID、旧引用保持未解析且不误绑；删除项不复用 ID；旧 cooked schema 返回 re-cook 错误。
- Asset tests：重复请求、dependency cycle、每个加载阶段的故障注入、cancellation、迟到完成消息、loading 期间 release、长期 lease 随 MeshRenderer 删除而释放，以及三类 GpuID 的 stale handle。
- World tests：create/destroy、generation、component query、deferred mutation、owning collection 在 component move/copy/remove/World exit 时的生命周期，以及 `destroyEntity`（children reparent 到 root）与 `destroySubtree`（整棵子树删除）的区别和 ChildOf 删除策略。
- Transform tests：深层 hierarchy、cycle rejection、reparent、parent deletion、**奇异 parent 下 `setParent(keepWorld)` 返回失败且 parent 不变**，以及 `LocalTransform`/`LocalMatrix` 互斥。
- Serialization tests：round trip、稳定输出、forward reference、非法 hierarchy、多版本 migration、reflected array resize/insert/remove、material override key 唯一且有序、同时带 `LocalTransform`+`LocalMatrix` 的非法文件，以及未知 component blob 保留。
- RenderScene tests：三层 slot reuse、instanceIndices 正确引用不连续 slot、一个多-submesh Entity 展开成多个 RenderDraw、asset-not-ready、entity removal 和无逐帧分配/逐 draw lease；轮换至少三份 backing storage 时，每次发布前都与目标 revision 一致，包括只修改一次的数据和删除/复用结果，覆盖索引、active list、计数、asset table 和 journal 溢出后的完整同步。
- Render history tests：移动后下一次实际渲染推进 previous；只 Update 不 Draw、多次 Update 后提交、丢弃 extraction 都不提前推进历史；首次出现、teleport、camera cut 和窗口恢复正确重置历史。
- Live graphics tests：pending GPU work 下释放 World/Asset、严格 shutdown 顺序，以及多次 init/exit。

增加包含至少 100,000 个简单 Entity 的 CPU stress case，用于发现非预期的 O(N squared) 路径和逐对象分配；性能数字作为 telemetry 记录，不作为跨机器固定断言。

Phase-1.5 随 hot reload 实现补充 PayloadCompatible/StructuralChange 测试：配对 snapshots 原子切换、依赖变化，以及 MeshRenderer 持续持有长期 lease 时反复重载，确认旧 payload 在其版本使用者和 GPU fence 结束后回收、当前 payload 与 GpuID 仍有效。这些不是第一阶段的验收前提。

## Definition of done

World Foundation 达到第一阶段完成标准时：

- Bistro 通过**一个 SceneAsset record + 各自去重的 Mesh/Material/Texture dependency record** 加载；这些资产 record 可被多个 Entity 共享（重复请求同一 `AssetID` 复用 record）。
- Asset Catalog、Registry 和 typed loaders 不依赖 `SceneManager`，旧 SceneManager 路径已从 Renderer Example 移除。
- Entity hierarchy 的编辑、保存和重新加载保持 ObjectID、asset reference 和 transform 结果。
- Cooked SceneAsset 保存可编辑 node hierarchy，sidecar 保存完整子资产身份表，离线重导入不会静默改绑已有引用；正式 World 路径不依赖旧的 flat `SceneAssetInstance` 适配。
- Renderer 只读取 RenderScene，不再依赖 SceneAsset 的 CPU instance 数组。
- ECS component 不持有裸 GPU resource pointer 或 transient descriptor index。
- stale asset/entity handle、serialization migration 和 hierarchy failure 有测试覆盖。
- Render extraction 正常帧无内存分配，各 backing storage 补齐累计 dirty ranges；首次使用、扩容和 journal 溢出可完整同步。配对快照版本一致，渲染历史只随实际提交推进。
- pending loader 和 GPU work 下按规定顺序销毁 World、Asset Registry、Renderer 和 RHI，不发生泄漏或 use-after-free。
- Reflection metadata 足以驱动后续 Editor 的 hierarchy、Inspector 和 property command。
