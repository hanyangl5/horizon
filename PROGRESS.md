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
