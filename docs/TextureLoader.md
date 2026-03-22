# Texture Loader / Texture Pipeline TODO

## 背景

当前 Horizon 的纹理链路已经具备基本的“解码 -> 创建 GPU 纹理 -> 上传 -> 运行时补 mip”能力，但还没有形成一条完整、稳定、可扩展的纹理资产管线。

现状大致是：

- `png/jpg/tga` 通过 `stb_image` 直接解码成 `RGBA8`
- `LoadFromMemory()` 也统一解码成 `RGBA8`
- 只有 `dds` 路径能够保留现成的 mip / 压缩格式
- 缺失 mip 时，在运行时通过 `GenerateMipMap()` 兜底

这套方案适合早期验证，但随着材质数量、分辨率、平台数增加，会逐渐暴露出显存、带宽、加载时间、质量一致性的问题。

## 当前问题

### 1. 缺少纹理语义层

当前加载逻辑主要按文件扩展名走，没有统一表达：

- BaseColor / Emissive 是 `sRGB`
- Normal / Roughness / Metallic / AO / ORM 是 `Linear`
- HDR 环境贴图需要高动态范围格式
- Mask / AlphaTest 贴图需要覆盖率友好的 mip 生成策略

结果是：

- mip 生成无法按贴图语义做不同处理
- 压缩格式选择缺乏依据
- 运行时无法稳定判断一个纹理是否允许走普通颜色路径

### 2. 运行时承担了过多烘焙职责

现在主路径里，原始图片经常直接在运行时解码并上传。缺失 mip 时再运行时生成。

这样的问题是：

- 首次加载成本高
- 纹理压缩收益吃不到
- mip 质量受限于运行时通用算法
- 法线、alpha-mask、HDR 这类特殊贴图无法被正确处理

### 3. 压缩格式支持虽然有基础，但没有形成资产流程

当前 `dds` 路径已经开始做格式映射，也能携带 mip 数据，但它还只是“支持读取一部分压缩贴图”，不是“引擎统一的纹理交付格式”。

缺失的关键环节包括：

- 统一的 texture cooker
- 纹理语义到平台格式的映射规则
- 离线生成 mip / 压缩的标准流程
- 运行时优先加载 cooked texture，而不是原图

### 4. 缺少平台化交付策略

从长期看：

- PC / Console 更适合 BCn
- 移动端更适合 ASTC / ETC2
- 统一产物更适合 KTX2 + BasisU

当前链路没有把“源资源”和“平台最终资源”分开。

## 一个合理的纹理流程应该是什么样子

推荐拆成 5 段：

### 1. 源资源导入

保留美术源文件格式：

- `png`
- `jpg`
- `tga`
- `hdr` / `exr`

导入时先识别纹理语义，而不是直接决定 GPU 格式。

建议统一语义枚举：

- `ColorSRGB`
- `LinearData`
- `NormalMap`
- `Mask`
- `HDRColor`
- `UI`

### 2. 离线烘焙（Texture Cooker）

新增一个离线纹理处理工具，负责：

- 解码源图
- 颜色空间处理
- 法线贴图重建 / renormalize
- alpha coverage preserve
- mip 链生成
- 平台压缩
- 生成最终可运行时加载的 cooked 纹理

这是整条链路的核心。mip 与压缩都应优先在这里完成。

### 3. 平台格式选择

建议按纹理用途选择目标格式：

- BaseColor / Albedo:
  - PC: `BC7_UNORM_SRGB`
- Emissive:
  - PC: `BC7_UNORM_SRGB`
- Normal:
  - PC: `BC5_UNORM`
- ORM / Roughness / Metallic / AO:
  - PC: `BC7_UNORM` 或按通道需求拆为 `BC4/BC5`
- Alpha Mask:
  - `BC4` / `BC7`
- HDR Cubemap / Environment:
  - `BC6H_UF16`
- UI / 小纹理 / 调试资源:
  - `RGBA8`

中长期跨平台建议：

- 主产物切到 `KTX2`
- 使用 `Basis Universal`
- 运行时按平台转码到 `BCn / ASTC / ETC2`

## 运行时应该负责什么

运行时尽量只做这些事：

- 读取 cooked texture header
- 创建正确格式 / 尺寸 / mip 数 / layer 数的 GPU 资源
- 一次性上传所有 subresource
- 转资源状态为 shader-readable

运行时不应长期承担：

- 主路径 mip 生成
- 主路径纹理压缩
- 主路径颜色空间纠正

运行时生成 mip 只保留给这些场景：

- 编辑器热加载
- debug 资源
- 外部临时资源
- 程序生成纹理

## Mip 处理建议

不是所有贴图都应该用同一种 mip 算法。

### BaseColor

- 先在线性空间 downsample
- 最终作为 `sRGB` 纹理输出

### Normal

- 不能直接当普通颜色平均
- downsample 后需要 renormalize
- 最终优先压成 `BC5`

### Mask / Alpha Test

- 不能只做普通 box filter
- 需要 coverage-aware mip，避免远处 foliage / fence 变稀

### ORM / Roughness / Metallic

- 通常可以线性 downsample
- 但 roughness 最好保留 perceptual 一致性，避免远处高光行为发散

### HDR

- 应保留高精度链路
- 最终压缩到 `BC6H` 或平台对应 HDR 格式

## 压缩流程建议

推荐顺序：

1. 解码源图
2. 做语义相关预处理
3. 生成完整 mip 链
4. 逐 mip 压缩
5. 写入统一容器格式和 metadata

需要写入的 metadata 至少包括：

- width / height / depth
- mip count
- layer count
- texture type（2D / Cube / Array）
- runtime format
- color space
- semantic
- alpha mode

## 对 Horizon 当前代码的建议改造

### 1. 拆分“源图解码”和“运行时纹理加载”

当前 `TextureLoader` 承担了两类职责：

- 从原始图片解码像素
- 从运行时可用格式构造上传描述

建议拆分为：

- `SourceTextureDecoder`
- `CookedTextureLoader`

这样职责更清晰，后续接入 `dds/ktx2` 也更自然。

### 2. 给纹理增加语义和颜色空间信息

建议在导入描述中显式记录：

- 是否 `sRGB`
- 是否 normal map
- 是否 alpha mask
- 是否 HDR

没有这层 metadata，后面的 mip / 压缩策略都无法稳定落地。

### 3. 把运行时 mip 生成降为 fallback

当前 `SceneManager` 在部分贴图缺失 mip 时会触发 `GenerateMipMap()`，这条逻辑可以保留，但应该变成兜底路径，不应是常态。

目标应该是：

- cooked 纹理默认自带完整 mip
- runtime mipgen 只处理少量特殊场景

### 4. 补齐常用压缩格式支持矩阵

优先补齐以下格式：

- `BC1`
- `BC3`
- `BC5`
- `BC6H`
- `BC7`
- 以及对应 `sRGB` 变体

### 5. 新增 texture cooker

建议做成独立工具，例如：

- `tools/texturecooker`

输入：

- 源纹理路径
- 纹理语义
- 平台目标
- 压缩配置

输出：

- `dds`（短期）
- `ktx2`（中长期）

## 推荐的分阶段落地方案

### Phase 1：先把 PC 主路径做对

目标：

- 保留现有源文件输入
- 新增离线 cooker
- 输出 PC 可直接使用的 `DDS`
- 所有主贴图离线生成 mip
- 所有主贴图离线压缩
- 运行时优先读取 cooked DDS

建议产物：

- BaseColor -> `BC7_SRGB`
- Normal -> `BC5`
- ORM -> `BC7_UNORM`
- HDR Env -> `BC6H`

### Phase 2：引入更通用容器

目标：

- 从 `DDS-only` 过渡到 `KTX2 + BasisU`
- 支持 PC / Android / Vulkan / DX12 的统一交付

### Phase 3：纹理流送

基础流程稳定后，再加：

- top mip 先加载
- 后台补高分辨率 mip
- residency / budget 管理

## 最小 MVP TODO

### 高优先级

- 定义纹理语义枚举与 metadata
- 拆分 `TextureLoader` 的职责
- 补齐常用 `DDS BCn` 读取支持
- 新增离线 texture cooker
- 让运行时优先读取 cooked texture

### 中优先级

- normal map 专用 mip 生成
- alpha coverage preserve
- HDR 贴图单独链路
- 统一 `sRGB / Linear` 标记策略

### 低优先级

- KTX2 + BasisU
- 纹理 streaming
- 平台差异压缩自动选择

## 一句话总结

合理的流程不是“运行时看到图片就解码、上传、补 mip”，而是：

`源纹理 -> 识别语义 -> 离线生成正确 mip -> 离线压缩成平台格式 -> 运行时直接加载 cooked 产物`

Horizon 当前最值得先做的，是把纹理系统从“文件读取器”升级成“纹理资产管线”。
