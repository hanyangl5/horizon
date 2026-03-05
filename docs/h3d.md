**Plan Mode 设计方案（h3d C 数学库替换 SimpleMath，先覆盖 vector/matrix）**

1. 目标与边界  
- 目标：在 `third_party` 新增 `h3d` C 数学库，替换当前 `SimpleMath` 依赖，先支持 `vector/matrix`。  
- 目标：尽量不改业务代码调用习惯，优先保证编译与行为一致。  
- 非目标：本阶段不处理完整 quaternion/plane/ray/color 高级能力，只保留最小兼容壳。  

2. 当前耦合点（你仓库里）  
- 核心入口在 [math.h](D:/Codes/horizon/framework/core/math.h)。  
- 两处直接调用 `DirectX::SimpleMath::Matrix::Create*` 在 [camera.cpp:48](D:/Codes/horizon/framework/scene/camera/camera.cpp:48)、[camera.cpp:60](D:/Codes/horizon/framework/scene/camera/camera.cpp:60)。  
- 大量代码依赖 C++ 运算符和成员函数（`Normalize/Cross/Dot/LengthSquared/Invert/Identity`），因此不能只上“纯 C 函数接口”，必须有 C++ 兼容层。  

3. 总体架构（三层）
- `Layer A: C Core`  
  `third_party/h3d_math` 提供纯 C 类型和函数，做真实数学计算。
- `Layer B: C++ Compat`  
  提供 `DirectX::SimpleMath` 同名最小子集（`Vector2/3/4/Matrix`），底层调用 C Core。
- `Layer C: Horizon Adapter`  
  保持 `Horizon::Math` 包装 API 不变，减少上层改动。  

4. 目录与文件规划  
- `third_party/h3d_math/include/h3d_math.h`  
- `third_party/h3d_math/src/h3d_math.c`  
- `third_party/h3d_math/include/h3d_simplemath_compat.hpp`  
- 可选：`third_party/h3d_math/tests/h3d_math_test.cpp`  

5. C Core 设计（Layer A）
- 类型  
  `h3d_vec2 {float x,y;}`  
  `h3d_vec3 {float x,y,z;}`  
  `h3d_vec4 {float x,y,z,w;}`  
  `h3d_mat4 {float m[4][4];}`  
- 约定  
  与现有行为保持一致：`row-major` 存储与当前乘法语义，不改 shader 侧约定。  
- 必选 API  
  `h3d_vec3_add/sub/mul_scalar/div_scalar`  
  `h3d_vec3_dot/cross/length/length_sq/normalize`  
  `h3d_mat4_identity/mul/transpose/invert`  
  `h3d_mat4_look_at`  
  `h3d_mat4_perspective_fov`  
  `h3d_mat4_perspective`  
  `h3d_mat4_orthographic`  
- 行为约束  
  归一化零向量返回零向量。  
  不可逆矩阵 `invert` 返回 identity（与现有 Android 包装一致）。  

6. C++ 兼容层设计（Layer B）
- 命名空间保持：`namespace DirectX::SimpleMath`。  
- 提供类型  
  `Vector2/Vector3/Vector4/Matrix`，包含当前工程已使用构造、字段、运算符、成员函数。  
- 必须覆盖的成员/操作  
  `Vector3::Normalize()` 与 `Normalize(out)`  
  `Vector3::Dot/Cross/Length/LengthSquared`  
  `+ - * / += -= *= /=`（向量与标量）  
  `Matrix::Identity`  
  `Matrix::operator*`、`*=`  
  `Matrix::Invert()`、`Transpose()`  
  `Matrix::CreateLookAt/CreatePerspectiveFieldOfView/CreatePerspective/CreateOrthographic`  
- 兼容占位  
  `Quaternion/Plane/Ray/Color/BoundingFrustum` 保留最小定义，先满足编译。  

7. Horizon 适配层（Layer C）
- 改造 [math.h](D:/Codes/horizon/framework/core/math.h)：  
  从 `SimpleMath.h` 切换到 `h3d_simplemath_compat.hpp`。  
  `using float2/3/4/float4x4` 不变。  
  `Math::Normalize/Cross/LookAt/Perspective/Ortho` 包装不变。  
- 清理 [camera.cpp](D:/Codes/horizon/framework/scene/camera/camera.cpp)：  
  两处直接 `DirectX::SimpleMath::Matrix::Create*` 改为走 `Math` 包装（降低未来耦合）。  

8. CMake 集成方案
- 修改 [third_party/CMakeLists.txt](D:/Codes/horizon/third_party/CMakeLists.txt)：  
  新增 `add_library(h3d_math STATIC ...)`。  
  `target_include_directories(h3d_math PUBLIC third_party/h3d_math/include)`。  
  `third_party` target 链接 `h3d_math`。  
- Windows 下不再构建/链接 `simplemath`（`directxtk12/src/SimpleMath.cpp`）。  
- Android/Windows 都统一走 `h3d_math`，移除平台分叉数学实现。  

9. 分阶段迁移计划
1. Phase 0（脚手架）  
创建 `h3d_math` 库与头文件，先不切换入口，确保库单独可编译。  
2. Phase 1（兼容层）  
实现 `h3d_simplemath_compat.hpp` 最小子集，补齐当前工程实际用到的 API。  
3. Phase 2（入口切换）  
切换 [math.h](D:/Codes/horizon/framework/core/math.h) 引用到兼容层；修正 [camera.cpp](D:/Codes/horizon/framework/scene/camera/camera.cpp)。  
4. Phase 3（验证）  
全量编译 `framework/unit_tests/samples`；跑场景烟测（相机、meshlet、骨骼）。  
5. Phase 4（清理）  
移除残余 `SimpleMath` include 与链接项，保留 directxtk 其它依赖（若有）。  

10. 验证与验收标准
- 编译验收  
  Windows Debug/Release 通过。  
  Android 构建通过。  
- 数值验收（epsilon 建议 `1e-5`）  
  `normalize(dot/cross)` 基础向量测试。  
  `mat * mat_inv ≈ identity`。  
  `LookAt/Perspective/Ortho` 与旧实现关键元素一致。  
- 集成验收  
  相机矩阵正确（无镜像/翻转/深度异常）。  
  网格/蒙皮无明显形变回归。  

11. 风险与控制
- 风险：矩阵约定（行列主序/左右手）不一致导致渲染错位。  
  控制：保持现有公式与乘法顺序，增加视图投影快照测试。  
- 风险：`float3` 内存布局变化影响 `memcpy` 路径。  
  控制：`static_assert(sizeof(Vector3)==12)` 且标准布局。  
- 风险：求逆实现差异导致动画/相机漂移。  
  控制：加入逆矩阵数值回归测试。  

12. Definition of Done
- `framework` 内不再依赖 `SimpleMath.h`。  
- `Math::float2/3/4/float4x4` 行为与现有使用点兼容。  
- CMake 中 `simplemath` 链接被 `h3d_math` 替代。  
- 单元测试与主要运行路径通过。  

如果你确认这个设计，我下一步就按这个 plan 执行 `Phase 0~2`，先提交“可编译替换版”，再补 `Phase 3` 测试。