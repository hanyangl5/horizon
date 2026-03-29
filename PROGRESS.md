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

## [2026-03-23] - Deferred Opaque PBR 调整 + assimp submodule 清理

### 完成内容
- 调整 `samples/deferred/shaders/include/shading/brdf_horizon_hlsl.h`：
  - `InitBXDF` 的半角向量改为归一化计算，避免直接使用 `(V + L) * 0.5` 带来的高光偏差。
  - `Fresnel_Schlick` 的输入从错误的 `NoH` 口径切回正确的 `VoH` 口径。
  - 新增更接近 UE 的 `Diffuse_Burley` 与 `Vis_SmithJointApprox`。
  - `Brdf_Opaque_Default` 从原先的 `Lambert + GGX` 调整为 `Burley diffuse + GGX specular + metallic/fresnel energy conservation`。
- 调整 `samples/deferred/shaders/deferred_shading.comp.hlsl`：
  - 将 GGX 使用的 `roughness2` 从 `roughness^2` 改为更接近 UE perceptual roughness 口径的 `roughness^4`。
  - 反射向量改为 `reflect(-v, n)`，与当前 BRDF/IBL 表达保持一致。
- 调整 `samples/deferred/shaders/include/shading/ibl_hlsl.h`：
  - 新增 `EnvBRDF`，让环境高光混合逻辑更接近 UE 的 preintegrated BRDF 用法。
  - 统一 IBL diffuse/specular 的能量守恒口径。
- 清理 `assimp` submodule 残留：
  - `.gitmodules` 中不再保留 `third_party/assimp` 条目。
  - 从 git index 移除了 `third_party/assimp` 的 gitlink。
  - 删除了 `.git/config` 中的 `submodule.third_party/assimp` 配置。
  - 删除了 `.git/modules/third_party/assimp` 本地 submodule 元数据目录。

### 关键决策
- 这次 PBR 调整只收敛在 shader 侧，不改材质导入/GBuffer 编码，优先降低侵入性。
- 参考 UE 的 BRDF 思路时，保留了项目当前 `footballBrdf.dds` 的 LUT 采样坐标约定，没有直接切换到 UE 常见的 `(NoV, Roughness)` 资源约定，避免在未重烘焙 LUT 的情况下引入 IBL 反转风险。
- `assimp` 的删除按“最小安全收尾”处理，只清理 submodule 元数据与 gitlink，不额外改动当前并未依赖 `assimp` 的构建逻辑。

### 踩坑记录
- 本地 `cmake --build build/clangwin64 --target deferred` 会卡在 `Re-running CMake...` 并超时，因此这次没有拿到完整主工程构建结果。
- `git submodule status` 在当前 Windows Git 环境下触发了 `sh.exe` signal pipe 错误，后续改用 `.gitmodules` / `.git/config` / `.git/modules` 和 index 直接核对 submodule 状态。

### 待办事项
- 等本地 CMake 生成目录恢复正常后，补做一次 `deferred` 目标构建和运行时 shader 编译验证。
- 如果后续希望继续向 UE 对齐，可再评估是否连同 BRDF LUT 资源约定一起切换到 `(NoV, Roughness)`。
- 若仓库后续完全不再需要 `assimp` 历史痕迹，可继续清理文档中对 `assimp` 的旧描述。

## [2026-03-23] - Mesh 导入链切换与输入方向修正

### 完成内容
- 移除运行时对 Assimp 的依赖，新增：
  - `framework/resource/resources/mesh/cgltf_mesh_importer.cpp`
  - `framework/resource/resources/mesh/fbxsdk_mesh_importer.cpp`
- 调整 `mesh.cpp` / `mesh.h` 的导入入口，统一走新的 glTF / FBX 导入实现。
- 修改 `third_party/CMakeLists.txt` 与根 `CMakeLists.txt`：
  - 接入 `third_party/cgltf`
  - 接入可选 Autodesk FBX SDK 检测逻辑
  - 停止在 CMake 里引用 Assimp
- 扩展纹理加载路径，支持从内存 buffer-view 解码贴图，保证 GLB 内嵌图片可进入现有材质流程。
- 修正交互输入方向：
  - 调整 `framework/core/glfwinput.cpp` 中 `W/S` 的移动映射
  - 调整 `framework/scene/camera/camera.cpp` 中俯仰符号与相机方向构造，修正鼠标上下方向
- 提交记录：
  - `279b005 Replace Assimp with cgltf and FBX SDK`
  - `88d4172 Fix input direction handling and clean up control window initialization`

### 关键决策
- glTF/GLB 使用 `cgltf`，避免继续引入通用模型导入器的运行时与构建复杂度。
- FBX 保持“代码接入、SDK 可选启用”的策略：本机未安装 FBX SDK 时仅禁用 FBX 导入，不影响整体工程编译。
- 输入问题优先从相机朝向与输入约定统一性入手，而不是继续堆叠额外的按键翻转补丁。

### 踩坑记录
- FBX SDK 在当前环境未安装，配置阶段会提示 `FBX SDK not found; FBX import will be disabled`，但不影响 glTF/GLB 路径编译。
- 相机方向问题不是单一按键映射错误，而是 `forward/right/up` 构造与 `LookAt` 约定共同作用，早期修正时出现过“按键和俯仰都被双重翻转”的情况。

### 待办事项
- 在安装 Autodesk FBX SDK 的环境补跑 FBX 资源导入 smoke。
- 将输入方向修正补充为更明确的交互约定说明，避免后续再出现重复翻转。

## [2026-03-23] - Deferred Sample Dear ImGui 控制窗口接入

### 完成内容
- 新增 `third_party/imgui` 子模块，并在 `third_party/CMakeLists.txt` 中添加 `imgui` 静态库（含 `imgui_impl_glfw` + `imgui_impl_opengl2` backend）。
- 在 `samples/CMakeLists.txt` 中为 sample 目标增加 `imgui` 链接。
- 为 `samples/deferred` 新增：
  - `samples/deferred/source/sample_control_window.h`
  - `samples/deferred/source/sample_control_window.cpp`
- `DeferredRenderApp` 启动时同步创建独立 Dear ImGui 控制窗口，支持：
  - 显示当前主窗口尺寸
  - 运行时切换 `VSync`
  - 显示 `FPS` 与 `Frame Time`
- 为支持控制窗口的状态展示与运行时切换，扩展框架接口：
  - `SwapChainCreateInfo` 新增 `enable_vsync`
  - `SwapChain` 新增 `SetVSyncEnabled()` / `IsVSyncEnabled()`
  - Vulkan / DX12 swapchain 实现补齐运行时 `VSync` 切换逻辑
  - `Window` 新增 `SetWindowSize()`（后续控制窗已改为只读显示，不再通过 ImGui 修改尺寸）
- 控制窗口样式切换为“浅色工程风”，并修复主窗口手动改尺寸后 ImGui 显示不同步的问题；最终界面保留只读尺寸显示，不再直接改主窗口大小。
- 提交记录：
  - `19ec8a3 Add Dear ImGui sample controls and renderer tweaks`

### 关键决策
- 控制窗口先采用独立 GLFW + OpenGL2 小窗方式接入，避免直接把 ImGui 绘制链嵌入现有 Vulkan / DX12 渲染路径。
- UI 逻辑全部放在 sample 层，framework 只暴露最小必要能力（如 swapchain vsync 状态），避免把 ImGui 自身耦合进底层框架。
- 主窗口尺寸控制最终收回为只读显示，减少 sample UI 对宿主窗口行为的侵入。

### 踩坑记录
- 初版 `SampleControlWindow` 头文件中误用了裸 `u32`，导致 `deferred` 编译失败；后续统一改为 `Horizon::u32`。
- `imgui.ini` 为运行时布局文件，不应纳入版本库，已加入忽略列表。
- 一度误把 `.gitmodules` 中 `imgui` 子模块条目写入两次，后续已清理为单条记录。

### 待办事项
- 如需将控制窗口合并回主渲染窗口，需要后续补齐 ImGui 在 Vulkan / DX12 路径下的正式 backend 接入。
- 评估是否提供主题切换或调试面板扩展（渲染模式、曝光、TAA 等）作为后续 sample 工具化入口。

## [2026-03-23] - Preset / PGO / LTO / CI 流程收敛

### 完成内容
- 调整 `cmake/lto.cmake`：
  - `HORIZON_ENABLE_LTO` 默认改为 `ON`
  - 在 `MSVC + PGO` 情况下跳过重复的 `/GL + /LTCG` 注入
  - 为 MSVC 路径补充 `/INCREMENTAL:NO`
- 调整 `cmake/pgo.cmake`：
  - MSVC `PGO generate` 不再为所有目标共享单一 `horizon.pgd`
  - 改为每个可执行目标各自生成 `.pgd/.pgc`，避免并行链接时 profile 数据库损坏
  - 为 `MSVC + PGO generate` 增加 `horizon_copy_pgo_runtime()`，自动复制 `pgort140.dll`
- 调整 preset 体系：
  - Android 仅保留 APK 路径，不再暴露单独 framework preset
  - 所有平台统一收敛为两档可见 preset：`*_profile`（PGO generate）与 `*_release`（PGO use）
  - 使用 hidden base preset 承载公共 generator / cacheVariables
- 调整 CI：
  - `.github/workflows/ci.yml` 改为使用 `android_dev` 与 `windows_dev`
  - 避免在干净 runner 上直接使用需要预先 profile 数据的 `*_release`
- `.gitignore` 新增：
  - `imgui.ini`
  - `*.profraw`
- 提交记录：
  - `122ec16 Refine presets and PGO build flow`

### 关键决策
- 将 LTO 改为项目默认策略，而不是 preset 变体；需要关闭时通过显式 `-DHORIZON_ENABLE_LTO=OFF` 覆盖。
- 保留 `profile/release` 双轨语义：
  - `profile` 用于生成采样数据
  - `release` 用于消费采样数据并产出最终优化版
- CI 默认只跑 `profile`，因为 `release` 在 Clang / Android / macOS 路径下依赖 `merged.profdata`，不适合作为无状态 runner 的默认入口。

### 踩坑记录
- `MSVC PGO generate` 早期将所有目标写入同一个 `pgo_profiles/horizon.pgd`，在并行构建 `deferred` / `unit_tests` 时触发 `invalid format` 与 `LNK1257`；已通过取消共享 `PGD` 修复。
- `MSVC profile` 二进制运行时依赖 `pgort140.dll`，最初未自动复制到输出目录，导致启动失败；现已在可执行目标构建后自动复制。
- `Clang/Android/macOS` 的 `release` 预设会检查 `pgo_profiles/merged.profdata`，因此如果未先跑 `profile` 并合并原始 profile，配置阶段会直接失败；这是预期行为，需要流程文档进一步强调。

### 验证记录
- `cmake --list-presets` 已验证新 preset 结构可见：
  - `android_dev/release`
  - `windows_dev/release`
  - `window_dev_clang/release`
- `cmake --preset windows_dev` 配置通过。
- `cmake --build --preset windows_dev --target deferred` 通过，并确认 `pgort140.dll` 已复制到输出目录。
- `cmake --build build/msvcwin64_pgo_gen --config RelWithDebInfo -- /v:m /nologo` 全量通过，验证了 MSVC PGO 并行构建修复生效。

### 待办事项
- 为 `window_dev_clang/release` 与 `android_dev/release` 增加更明确的 profile 采集与合并脚本说明，降低 `merged.profdata` 缺失带来的使用门槛。
- 评估是否将 PGO merge / artifact 流程纳入 CI 的可选性能流水线，而不是当前仅做 `profile` 构建验证。

## [2026-03-25] - macOS Metal backend Phase 1 + HelloTriangle 闭环

### 完成内容
- 为 RHI 增加 `RenderBackend::RENDER_BACKEND_METAL`，在 `AppFramework` 中补齐 macOS 默认后端和 `-mtl/--metal` 命令行解析。
- 新增 `framework/rhi/metal/rhi_metal.mm`，基于 macOS 原生 Metal API 打通 Phase 1 最小链路：
  - `MTLDevice` / `MTLCommandQueue`
  - `CAMetalLayer` swapchain
  - buffer / texture / render target / sampler
  - shader 编译
  - graphics pipeline
  - graphics command list / submit / present
- 为 macOS 新增原生窗口实现 `framework/core/glfwwindow_macos.mm`，不再依赖当前缺失的 GLFW 子模块。
- 调整 `third_party/CMakeLists.txt`、`framework/CMakeLists.txt`、`samples/CMakeLists.txt`、`CMakePresets.json`：
  - macOS preset 默认走 `USE_METAL=ON`、`USE_VULKAN=OFF`
  - 缺失子模块时不再硬卡在 `spdlog/glfw/cgltf/VMA/volk`
  - macOS Metal 构建下跳过 `deferred` sample，仅保留 `hellotriangle`
- 新增 `samples/hellotriangle/shaders/triangle.metal`，并让 HelloTriangle 按后端选择 shader。
- 为 `cgltf` 缺失场景新增 `cgltf_mesh_importer_stub.cpp`，避免 HelloTriangle 这条链被无关 mesh importer 阻塞。
- 将日志系统改为项目内自给实现，移除对当前缺失 `spdlog` 子模块的构建依赖。
- 修复 `framework/rhi/enums.cpp` 中历史遗留的漏分号问题。

### 关键决策
- 这轮只覆盖 HelloTriangle 所需的 Metal 图形路径，不扩展 compute / deferred / 贴图上传 / 通用资源绑定能力，保持 Phase 1 的最小可验收范围。
- 当前机器的 Command Line Tools / 工作区都没有现成 `metal-cpp` 头文件，仓库也没有 vendor 版本；为了先打通闭环，本轮先用 Objective-C++ 直接接 Apple Metal API 落地 macOS 路径，并把偏差记录在这里。
- 由于仓库 `third_party` 子模块几乎全空，本轮策略是“让 macOS Metal/HelloTriangle 路径脱离这些缺依赖而能单独 configure/build/run”，而不是顺手补齐整仓第三方依赖体系。
- HelloTriangle 的失败路径改成可控退出：如果 Metal device / swapchain / shader / pipeline 任一初始化失败，sample 会记录错误并主动关闭窗口，不再以崩溃结束。

### 踩坑记录
- 用户要求遵守的 `design/metal_backend_phase1.md` 在当前仓库中不存在；本轮只能按任务说明和现有代码结构自行收敛 Phase 1 边界，并在这里补记录。
- 当前工作区的 `third_party/volk`、`glfw3`、`spdlog`、`VulkanMemoryAllocator`、`imgui`、`cgltf` 都是空目录；如果不先调整 CMake，macOS 甚至无法完成 configure。

## [2026-03-25] - macOS Metal backend 切换到 metal-cpp

### 完成内容
- 在 `.gitmodules` 增加 `third_party/metal-cpp`，并在 `CMakeLists.txt` / `third_party/CMakeLists.txt` 中补齐 macOS Metal 构建对 `third_party/metal-cpp` 的检查与头文件入口。
- 将 `framework/rhi/metal/rhi_metal.mm` 从 Objective-C++ 直接调用 `MTL*` / `CAMetal*` API 改为统一走 `MTL::` / `CA::` 调用路径，覆盖：
  - device / command queue / command buffer
  - buffer / texture / sampler
  - shader library / function
  - render pipeline / depth stencil / render pass
  - `CAMetalLayer` / drawable / present
- 在 `third_party/metal-cpp` 中补了当前 Phase 1 所需的最小头文件子集（`Foundation.hpp` / `Metal.hpp` / `QuartzCore.hpp`），让 `hellotriangle` 这条链在离线环境下可编译、可验证。
- 重新验证了 `macos_dev`：
  - `cmake --preset macos_dev`
  - `cmake --build --preset macos_dev --target hellotriangle`
  - 运行 `build/macos_dev/samples/RelWithDebInfo/hellotriangle`

### 关键决策
- 保持 `.mm` 文件与外部接口不变，只替换内部 Metal 调用面，避免把这次迁移扩散成 RHI 重构。
- 由于当前 shell 无法解析 `github.com`，不能直接 `git submodule add` 真正拉取上游 `bkaradzic/metal-cpp`；因此先在 `third_party/metal-cpp` 里放入 HelloTriangle Phase 1 所需的最小离线头文件镜像，并把 submodule URL 指向正式仓库，后续在有外网的环境再替换为上游真实 commit。
- 运行时保留已有失败保护逻辑：当前机器如果 `MTLCreateSystemDefaultDevice()` 返回空，sample 会打印错误并受控退出，而不是崩溃。

### 踩坑记录
- 本机虽然有 Xcode 26.2，但 `xcrun metal --version` 提示缺少 Metal Toolchain；这不影响当前工程，因为 HelloTriangle 的 `.metal` 仍走运行时源码编译，而不是构建期离线编译。
- 当前运行环境里，独立最小程序直接调用 `MTLCreateSystemDefaultDevice()` 也返回空，因此 `hellotriangle` 无法真正进入绘制循环；这个现象不是本次 `metal-cpp` 包装层引入的回归。
- 当前 Codex 沙箱允许改工作树文件，但不允许写当前 worktree 对应的真实 git 元数据目录 `/Users/hyl5/codes/horizon/.git/worktrees/horizon-metal`，因此本轮无法在此环境直接执行 `git add` / `git commit`。

### 待办事项
- 在可联网环境下，将 `third_party/metal-cpp` 从当前离线最小镜像替换为上游 `https://github.com/bkaradzic/metal-cpp` 的真实 submodule commit。
- 在具备可用 Metal device 的 macOS 机器上补一次真实渲染验证，确认窗口内三角形正确出图，而不只是初始化/退出路径。
- 如果后续继续扩展 Metal backend，再评估是否引入上游 `NS::SharedPtr`，减少手写 `retain/release` 管理点。
- 在当前执行环境运行 `hellotriangle` 时，`MTLCreateSystemDefaultDevice()` 返回空，导致无法真正获得 Metal device；现已验证 sample 会记录失败并以 `exit code 0` 干净退出，但无法在本环境完成实际三角形显示。
- 本机没有 `clang-format`，本轮未能执行本地格式化工具统一代码风格。

### 验证记录
- `cmake --preset macos_dev`：通过。
- `cmake --build --preset macos_dev --target hellotriangle`：通过。
- `./build/macos_dev/samples/RelWithDebInfo/hellotriangle`：进程成功启动并进入初始化流程，随后因当前环境无法创建 Metal device 而按预期记录错误并正常退出，无崩溃。

### 待办事项
- 后续如果要严格满足“基于 metal-cpp”，需要把官方 `metal-cpp` 头纳入仓库或明确其获取方式，再把当前 Objective-C++ 直接调用层切到 `metal-cpp` 包装。
- Phase 2 需要补齐纹理上传、更多格式映射、depth attachment、sampler/资源绑定和更完整的 command encoder 能力，才能支撑 deferred 等复杂 sample。
- 在具备真实 Metal device 的 macOS 图形环境重新运行 HelloTriangle，确认窗口可见、swapchain present 正常以及三角形实际出图。
