# Link Time Optimization (LTO) PRD

## 背景

当前 Horizon 在 Release 构建下已具备常规编译优化，但跨翻译单元优化能力有限（内联、去虚调用、全局死代码消除等仍受边界限制）。
LTO 可在链接阶段做全程序优化，通常能提升 CPU 热路径性能并减小二进制体积。

目标平台：
- Clang：macOS / iOS / Android
- MSVC：Windows

---

## 目标

- 在不破坏现有构建流程的前提下接入可控的 LTO 基础设施
- 提供统一 CMake 开关，支持按平台启用/禁用
- 与 Development / Shipping / PGO 兼容（尽量避免配置冲突）
- 在 CI 可验证至少 configure + build（可用平台）通过

## 非目标

- 不在本阶段做性能结论（只提供能力，不承诺固定收益）
- 不强制所有 preset 默认开启 LTO
- 不引入新的发布流程系统（符号服务器、打包系统不在本 PRD 范围）

---

## 方案设计

### 1) CMake 选项

新增：

| 变量 | 默认 | 说明 |
|---|---|---|
| `HORIZON_ENABLE_LTO` | `OFF` | 全局开关 |
| `HORIZON_LTO_MODE` | `AUTO` | `AUTO` / `THIN` / `FULL` |

行为：
- `AUTO`：Clang 优先 `ThinLTO`，MSVC 使用 `/GL + /LTCG`
- `THIN`：仅 Clang 有效（`-flto=thin`），MSVC 回退 `FULL`
- `FULL`：Clang `-flto`，MSVC `/GL + /LTCG`

### 2) 编译器实现

#### Clang
- 编译与链接统一追加：
  - ThinLTO: `-flto=thin`
  - Full LTO: `-flto`
- 保留 `-g`（若 build type 需要）
- Android 使用 NDK toolchain 自带 linker（lld）

#### MSVC
- 编译：`/GL`
- 链接：`/LTCG`
- 若 Shipping：可叠加 `/OPT:REF /OPT:ICF`

### 3) 与 PGO / Build Modes 协同

优先原则：
- 不改变现有 PGO 语义（PGO 开关优先）
- LTO 与 PGO 可共存（按工具链可用性）
- Development 模式允许 LTO 但默认关闭；Shipping 推荐开启

冲突处理：
- 在 configure 阶段做提示（`message(STATUS/WARNING)`），避免静默冲突

### 4) 代码组织

新增文件：
- `cmake/lto.cmake`：封装 LTO 选项与 flag 注入

修改文件：
- `CMakeLists.txt`：include `cmake/lto.cmake`
- `CMakePresets.json`：新增/扩展 LTO 预设
- `README.md` / `docs/build_modes_prd.md`：补充 LTO 使用说明

---

## Preset 规范

新增建议：
- `macos_lto`
- `android_framework_lto`
- `android_app_lto`
- `msvcwin64_lto`

可选组合（后续）：
- `*_shipping_lto`
- `*_pgo_use_lto`

---

## 实施计划

### Phase 1（基础能力）
- 新增 `cmake/lto.cmake`
- 根 CMake 接入
- 增加 4 个基础 LTO preset

### Phase 2（协同与校验）
- 与 PGO 选项共存校验（提示信息）
- 对可用平台执行 configure + build smoke

### Phase 3（文档与维护）
- README 与 docs 补充
- PROGRESS.md 记录决策/风险

---

## 验收标准

- `HORIZON_ENABLE_LTO=ON` 时，各平台生成构建命令含对应 LTO 参数
- 默认配置不受影响（不开 LTO 时行为与当前一致）
- 至少一个 Clang 平台与一个 MSVC 平台（或可用替代）通过 configure + build smoke
- 文档包含开关说明、preset 名称、已知限制

---

## 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| 链接时间显著增加 | CI 时长上升 | Development 默认关闭；优先 ThinLTO |
| 工具链版本差异导致链接失败 | 平台构建不稳定 | configure 阶段检查并给出明确 warning |
| 与 PGO/调试符号组合复杂 | 难排障 | 文档明确推荐组合与禁用组合 |

---

## 待确认项

- Android 是否默认采用 ThinLTO（建议是）
- Shipping preset 是否默认启用 LTO（建议启用）
- CI 是否增加单独 LTO job（建议先不加，先本地和手动验证）
