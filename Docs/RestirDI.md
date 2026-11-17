# ReSTIR DI

`Examples/Renderer` 的 deferred lighting 已经替换成 ReSTIR DI 的骨架。资源层（G-buffer 读取、逐帧灯光表、reservoir 历史、常量块、bindings）
已经就绪，四个算法阶段留空，等你自己填。

## 当前状态

- `RestirDI.slang`：
  - `loadSurface`、`loadMotion`、`makeLightingBindings`、`evaluateAmbientOcclusion`、`loadReservoir`、`storeReservoir`
    和哈希 RNG 已实现，可以直接用。
  - `initialCandidates`、`temporalReuse`、`spatialReuse`、`shadeReservoir` 是空的，函数签名已经带好所需资源。
  - `kUseReSTIRDI` 为 `false`；此时走 `referenceDirectLighting`（对灯光表暴力求和的参考解），画面正常可看，也是后面验证
    reservoir 路径的 ground truth。
- `RestirDI.cpp/h`：
  - 每帧从 `RenderScene` 的活动灯光重建 `RestirDILight` 表，三帧在飞的环（`Lights`），reservoir 也是三帧环（`Reservoirs`）。
  - `Resize` 后第一次录制会清零 reservoir 历史。
- `Renderer.h`：`CreateDemoLights` 在场景中心周围摆了 12 个点光源。ReSTIR DI 需要多光源，先用它们做测试；换成场景资产里的灯也可以。

## 资源与约定

| 项目 | 位置 | 说明 |
| --- | --- | --- |
| 灯光表 | `lightsIndex` → `StructuredBuffer<RestirDILight>` | 每帧重建，最多 `RestirDI::MaxLights` 个；`type` 是 `hz::LightType` |
| Reservoir | `previousReservoirIndex` / `outputReservoirIndex` | 64 字节/像素，线性索引 `pixel.y * width + pixel.x` |
| GBuffer | `emissiveIndex`、`normalMaterialIndex`、`baseColorMetallicIndex`、`depthIndex`、`motionIndex` | 通道含义见 [GBuffer](GBuffer.md) |
| 帧常量 | `frameIndex`、`lightingFrameIndex` | `FrameData`（相机矩阵）和 `LightingFrameData`（太阳、阴影、IBL 标定） |
| 阴影/AO | `shadowMapIndex`、`rtShadowAOIndex`、`ssaoIndex` | 都可能为 `INVALID_TEXTURE`，用前判断 |
| 环境 | `environmentIndex`、`irradianceIndex`、`brdfLutIndex` | 间接光只走 IBL，不走 reservoir |

Reservoir 字段（`RestirDIReservoir`，CPU 与 Slang 两侧都是 64 字节）：

| 字段 | 内容 |
| --- | --- |
| `samplePoint` | xyz 为光源上的采样点（世界空间），w 为 target pdf |
| `sampleNormal` | xyz 为发光面法线，w 为样本的立体角 pdf |
| `sampleRadiance` | rgb 为接收点处的入射辐亮度，w 为 reservoir 权重 `W` |
| `lightIndex` | 采到的光源下标 |
| `sampleCount` | `M` |

运动矢量约定：`motion` 是 UV 空间的 `uv_current - uv_previous`，因此 `previousUv = uv - motion`。

## 实现步骤

### 0. 打开分支

把 `RestirDI.slang` 里的 `kUseReSTIRDI` 改成 `true`。在此之前先实现第 1、4 步，否则画面会变成纯 IBL。

### 1. Initial candidates（`initialCandidates`）

对 `M`（建议 1～32）个候选：

1. 均匀选灯：`lightIndex = min(uint(restirDINextRandom(random) * lightCount), lightCount - 1)`。
2. 在灯上采样一个点，得到从接收点指向该点的单位方向 `direction`、距离 `distance`。
   - Directional：`direction = -normalize(light.direction.xyz)`，`radiance = light.color.rgb`。
   - Point：`direction = normalize(light.position.xyz - surface.position)`，`distance = length(...)`，
     `radiance = light.color.rgb / max(distance * distance, 1e-4)`；超出 `light.position.w`（range）要丢弃。
   - Spot：在 Point 的基础上，`cosAngle = dot(-direction, light.direction.xyz)`，小于 `light.direction.w`（cosOuter）
     丢弃，再乘 `smoothstep(cosOuter, cosInner, cosAngle)`。
   - Rect/Disk/Sphere/Cylinder：`lwidth`、`lheight`、`area` 已经准备好，按面积均匀采样后把面积 pdf 转成立体角
     pdf：`pω = (1 / area) * distance² / max(dot(lightNormal, -direction), 1e-4)`。
3. 计算 `NdotL = saturate(dot(surface.normal, direction))`；`NdotL <= 0` 丢弃。
4. 计算 target function：

   ```slang
   float3 f = pbrMetallicRoughness(surface.baseColor, surface.metallic, surface.roughness,
                                   surface.normal, v, direction,
                                   pbrF0(surface.baseColor, surface.metallic, surface.specular));
   float  pHat = dot(f * radiance, float3(0.2126, 0.7152, 0.0722));
   ```

5. 样本 pdf：`p = (1 / lightCount) * pLightSample`；delta 灯的 `pLightSample = 1`。
6. 用流式合并累加：`w = pHat / p`，`wSum += w`，然后以 `w / wSum` 的概率把该候选写进 reservoir 的
   `samplePoint`、`sampleNormal`、`sampleRadiance`、`lightIndex`。
7. `M` 个候选结束后收尾：

   ```slang
   reservoir.sampleCount = M;
   reservoir.sampleRadiance.rgb = selectedRadiance;
   reservoir.sampleRadiance.w = reservoir.sampleRadiance.w / (M * reservoir.samplePoint.w);
   ```

   （`samplePoint.w` 放的是所选样本的 target pdf，`sampleRadiance.w` 先放 `wSum`。）

### 2. Temporal reuse（`temporalReuse`）

1. `previousUv = uv - motion`；越界或 `any(previousUv < 0 || previousUv > 1)` 时只保留当前 reservoir。
2. 从 `loadReservoir(pixel, previousReservoirIndex)` 读上一帧的 reservoir；`sampleCount == 0` 时同样跳过。
3. 有效性检查（建议全做）：
   - 上一帧 reservoir 的 `lightIndex >= lightCount` 丢弃；
   - 用 `previousUv` 重建上一帧世界位置，和当前 `surface.position` 比较平面距离/法线夹角；
   - 材质 ID（`motionIndex` 的 A8）不同则丢弃。
4. 在当前像素重新求值上一帧 reservoir 的样本（方向和 BRDF 都要用当前表面重算），得到 `pHatPrevious`。
5. 合并：`wCurrent = pHatCurrent * M_current`，`wPrevious = pHatPrevious * M_previous`，按
   `wCurrent / (wCurrent + wPrevious)` 概率选择；`M` 上限 `min(20 * M_current, 上限)`。
6. 重新收尾 `W = wSum / (M * pHatSelected)`。

### 3. Spatial reuse（`spatialReuse`）

1. 在当前像素周围取 `K`（建议 5～8）个邻域像素，radius 建议 8～30 像素，带随机偏移。
2. 每个邻域 reservoir：
   - `sampleCount == 0` 跳过；
   - 用它存的光源样本对**当前像素**重新求值（方向、`NdotL`、`f`、`radiance`）；
   - 深度/法线差异过大跳过（几何一致性）。
3. pairwise MIS 合并。对候选 `i` 和邻域 `j`：

   ```slang
   float mi = pHatI * M_i;
   float mj = pHatJ * M_j;
   float mis = mi / (mi + mj);
   ```

   以 `mis * w_j / (wSum + mis * w_j)` 的概率接收 `j`。
4. 收尾方式与第 1 步相同。

### 4. Shading（`shadeReservoir`）

最简单且正确的形式是直接返回 reservoir 已经带好的贡献：

```slang
return reservoir.sampleRadiance.rgb * reservoir.sampleRadiance.w;
```

如果要把 visibility 留到最后再乘（阴影/AO 只影响直射光），在这里重新求值样本更合适：

```slang
float visibility = evaluateShadowVisibility(surface.position, pixel, light, bindings);
return f * radiance * visibility * reservoir.sampleRadiance.w;
```

### 5. 验证

1. 先让 `kUseReSTIRDI = false` 存一张参考图，再打开对比；两边应该没有系统性亮度差。
2. 单帧 `M = 1`、关闭 temporal/spatial 时，结果应接近参考图加噪声。
3. 相机静止时 temporal reuse 应明显降噪；移动相机时不应出现拖影（拖影说明重投影有效性检查不够）。
4. 用 GPU timestamp 看 `RestirDI` 的耗时；参考路径是 O(lights) 全量求和，reservoir 路径应该是 O(1) + 邻居数。

## 已知缺口

- 局部光源还没有阴影（`referenceDirectLighting` 里可见性恒为 1）。要么在候选生成时发射线，要么在 shading 时做 shadow ray。
- Rect/Disk/Sphere/Cylinder 目前只有数据，没有面积采样和参考求值，先按 Point 近似即可跑通。
- 环境光没有作为可采样光源进入 reservoir，仍走 IBL 解析积分。
- 空间复用只有单帧 reservoir，没有降噪器；需要的话可以加 a-trous（参考 `RTDenoise.slang`）。
- 每帧灯光表按 `CpuToGpu` 缓冲直传，最多 256 盏；超过会截断并打 warning。

## 改动清单

| 文件 | 用途 |
| --- | --- |
| `Examples/Renderer/Shaders/RestirDI.slang` | 四个算法阶段和 `kUseReSTIRDI` 在这里 |
| `Examples/Renderer/Source/RestirDI.h` | `RestirDILight` / `RestirDIReservoir` 的 CPU 侧布局 |
| `Examples/Renderer/Source/RestirDI.cpp` | 灯光表上传、reservoir 环、root constants |
| `Examples/Renderer/Source/Renderer.h` | `CreateDemoLights` 的测试灯 |

Reservoir 字段是你自己的设计，可以按需要改；CPU 和 Slang 两侧各有一个 `static_assert`，再加字段时把常量块
`RestirDIConstants` 和 `RestirDI.slang` 的 `cbuffer` 同步（两边字段顺序必须一致）。
