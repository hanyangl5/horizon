## [2026-03-06] - h3d 数学迁移 Phase 0~2

### 完成内容
- 新增 `third_party/h3d_math`：
  - `include/h3d_math.h`：C Core 类型与函数声明
  - `src/h3d_math.c`：Vector3 与 Matrix 核心计算实现（含 LookAt/Perspective/Ortho/Invert）
  - `include/h3d_simplemath_compat.hpp`：`DirectX::SimpleMath` 最小兼容层
- 修改 `third_party/CMakeLists.txt`：新增 `h3d_math` 静态库并将其作为 `third_party` 的 `PUBLIC` 依赖，移除 `simplemath` 链接与 `directxtk12/Inc` include 入口。
- 修改 `framework/core/math.h`：统一改为包含 `h3d_simplemath_compat.hpp`。
- 修改 `framework/scene/camera/camera.cpp`：两处 `DirectX::SimpleMath::Matrix::Create*` 直接调用改为通过 `Math`/兼容层调用。

### 关键决策
- 采用三层最小落地：C Core（`h3d_math.c`）+ C++ 兼容层（`h3d_simplemath_compat.hpp`）+ Horizon 适配层入口切换（`math.h`）。
- 兼容层保持 `DirectX::SimpleMath` 命名与常用操作符，尽量保证上层调用无感迁移。
- `h3d_math` 通过 `third_party` 的 `PUBLIC` 链接暴露头文件，避免在 `framework` 额外增加 include 配置。

### 踩坑记录
- 当前仓库第三方子模块未初始化（`volk/assimp/glfw3/spdlog/VulkanMemoryAllocator`），导致预设和本地 CMake 全量配置均无法完成。
- 在无法全量构建条件下，补充了独立编译烟测：
  - `cc -std=c11 -Ithird_party/h3d_math/include -c third_party/h3d_math/src/h3d_math.c`
  - `c++ -std=c++17 -Ithird_party/h3d_math/include /tmp/h3d_simplemath_smoke.cpp /tmp/h3d_math.o`
  - 可执行程序运行通过（`SMOKE_OK`）。

### 待办事项
- 拉取并初始化第三方子模块后，执行完整预设构建与单元测试。
- 在 Windows/macOS/Android 三端补齐矩阵数值回归测试（尤其 `Invert` 与投影矩阵关键元素一致性）。
- 后续 Phase 3/4 清理残余兼容路径（如 `android_simplemath.h` 的历史文件处理策略）。

## [2026-03-06] - PGO 基础设施接入（CMake/Preset/脚本）

### 完成内容
- 新增 `cmake/pgo.cmake`，提供 `HORIZON_PGO_GENERATE`、`HORIZON_PGO_USE`、`HORIZON_PGO_PROFILE_DIR` 三个 CMake 选项。
- 在根 `CMakeLists.txt` 引入 `cmake/pgo.cmake`，统一对全工程生效。
- 新增 PGO preset：`macos_pgo_gen/use`、`android_pgo_gen/use`、`msvcwin64_pgo_gen/use`，并补充对应 build preset。
- 新增 `tools/pgo_merge.sh`（可执行）用于 Clang `.profraw` 合并为 `merged.profdata`。
- 更新 `docs/pgo.md` 与 `README.md`，补齐脚本和流程联动。
- 完成可执行验证：`cmake --list-presets` 可见新增 PGO preset（含 macOS/Android）；临时 smoke 工程复用 `cmake/pgo.cmake` 跑通 `HORIZON_PGO_GENERATE -> tools/pgo_merge.sh -> HORIZON_PGO_USE` 全链路。

### 关键决策
- 将 PGO 逻辑放在独立 `cmake/pgo.cmake`，避免污染已有模块并保持最小入侵。
- `HORIZON_PGO_GENERATE` 与 `HORIZON_PGO_USE` 互斥，配置阶段直接失败，避免误配。
- Clang `use` 阶段在配置时检查 `merged.profdata` 是否存在，提前暴露问题。

### 踩坑记录
- 仓库初始缺少 `PROGRESS.md`，按 `CLAUDE.md` 要求本次任务中补建并记录。
- 当前工作区缺失多个 third_party 子模块（`volk/assimp/glfw3/spdlog/VulkanMemoryAllocator`），导致 Horizon 主工程无法在本机完成完整 configure/build；已改用最小 smoke 工程验证 PGO 基础设施行为。

### 待办事项
- 增加一个可自动执行的 PGO smoke 流程（采样 workload + merge + use 构建）用于 CI。
- 在 Android 真机场景补充 profile 采集/回传脚本，减少手工 adb 操作。

## [2026-03-06] - LTO 基础设施接入（CMake/Preset/文档）

### 完成内容
- 新增 `cmake/lto.cmake`：
  - 增加 `HORIZON_ENABLE_LTO`（默认 `OFF`）和 `HORIZON_LTO_MODE`（`AUTO/THIN/FULL`）
  - Clang 注入 `-flto=thin`/`-flto`（编译+链接）
  - MSVC 注入 `/GL` + `/LTCG`，并对 `THIN` 模式回退到 `FULL`
  - 增加模式校验与配置期提示（含 LTO+PGO 组合提示）
- 根 `CMakeLists.txt` 接入 `include(cmake/lto.cmake)`。
- 扩展 `CMakePresets.json`：新增 `android_framework_lto`、`android_app_lto`、`macos_lto`、`msvcwin64_lto` 及对应 build presets。
- 文档联动：
  - `README.md` 增加 LTO 文档入口
  - `docs/build_modes_prd.md` 增补 Build Mode 与 LTO 协同说明
  - `docs/pgo.md` 增补 PGO 与 LTO 组合说明和示例 preset

### 关键决策
- LTO 采用独立 `cmake/lto.cmake`，保持与 `cmake/pgo.cmake` 解耦，最小侵入接入根 CMake。
- `AUTO` 在 Clang 下默认走 ThinLTO（兼顾优化收益与链接时长），符合 PRD 建议。
- 与 PGO 组合时仅提示不改变 PGO 行为，满足“PGO 开关优先、可共存”。

### 踩坑记录
- Horizon 主工程配置仍受缺失子模块阻塞（`third_party/volk`），无法在当前环境完成全量构建验证。
- 为保证闭环，补充了最小 smoke 工程验证：确认 `-flto=thin` 与 `-flto` 均实际进入编译与链接命令并可成功产出可执行文件。

### 待办事项
- 初始化 third_party 子模块后，补跑 `macos_lto`/`android_*_lto`/`msvcwin64_lto` 的主工程 configure + build smoke。
- 在 Windows 环境补充 `/GL` + `/LTCG` 的实机构建验证记录。
- 评估是否新增 `*_pgo_use_lto` 组合 preset 作为后续优化入口。
