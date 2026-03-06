# h3d 数学库迁移 PRD

## 背景

当前项目在 Windows 上依赖 `DirectX::SimpleMath`（DirectXTK12 子集），Android 上有单独包装层，导致：

1. **平台分叉**：数学实现路径不统一，维护两套代码
2. **平台锁定**：`SimpleMath` 依赖 DirectXTK12，不能直接用于 macOS / iOS
3. **依赖冗余**：引入完整 DirectXTK12 仅为使用向量 / 矩阵类型
4. **耦合过深**：`camera.cpp` 直接调用 `DirectX::SimpleMath::Matrix::Create*`，而非走统一适配层

目标：引入轻量级跨平台 C 数学库 `h3d`，替换 `SimpleMath` 依赖，先覆盖 `vector / matrix`，统一所有平台的数学实现。

---

## 目标

- 移除对 `DirectX::SimpleMath` 的直接依赖
- 统一 Windows / Android / macOS / iOS 的数学实现路径
- 不改动上层业务代码的调用习惯（`Math::float2/3/4/float4x4` API 不变）
- CMake 中 `simplemath` 链接被 `h3d_math` 替代

## 非目标

- 本阶段不处理完整 `Quaternion / Plane / Ray / Color / BoundingFrustum` 高级能力，仅保留最小编译占位
- 不改动 shader 侧矩阵约定（行列主序 / 坐标系不变）

---

## 架构设计（三层）

```
Layer C: Horizon Adapter       framework/core/math.h
         Math::float2/3/4/float4x4 包装 API 不变

Layer B: C++ Compat            third_party/h3d_math/include/h3d_simplemath_compat.hpp
         DirectX::SimpleMath 同名最小子集
         底层调用 C Core

Layer A: C Core                third_party/h3d_math/include/h3d_math.h
                               third_party/h3d_math/src/h3d_math.c
         纯 C 类型与函数，做真实数学计算
```

---

## C Core 设计（Layer A）

### 类型

```c
typedef struct { float x, y; }          h3d_vec2;
typedef struct { float x, y, z; }       h3d_vec3;
typedef struct { float x, y, z, w; }    h3d_vec4;
typedef struct { float m[4][4]; }       h3d_mat4;
```

存储约定：与现有代码保持一致（row-major，不改 shader 侧约定）。

### 必选 API

| 类别 | 函数 |
|------|------|
| Vector3 算术 | `add / sub / mul_scalar / div_scalar` |
| Vector3 几何 | `dot / cross / length / length_sq / normalize` |
| Matrix | `identity / mul / transpose / invert` |
| Matrix 构造 | `look_at / perspective_fov / perspective / orthographic` |

### 行为约束

- `normalize` 零向量返回零向量
- 不可逆矩阵 `invert` 返回 identity（与现有 Android 包装行为一致）

---

## C++ 兼容层设计（Layer B）

命名空间保持 `namespace DirectX::SimpleMath`，提供当前工程实际用到的最小子集：

### 必须覆盖

| 类型 | 成员 / 操作 |
|------|-----------|
| `Vector2/3/4` | 构造、字段、`+  -  *  /  +=  -=  *=  /=`（向量与标量） |
| `Vector3` | `Normalize()` / `Normalize(out)`、`Dot`、`Cross`、`Length`、`LengthSquared` |
| `Matrix` | `Identity`、`operator*`、`*=`、`Invert()`、`Transpose()` |
| `Matrix` 静态构造 | `CreateLookAt`、`CreatePerspectiveFieldOfView`、`CreatePerspective`、`CreateOrthographic` |

### 编译占位（本阶段不实现）

`Quaternion / Plane / Ray / Color / BoundingFrustum` 保留最小定义，满足编译即可。

---

## Horizon 适配层改动（Layer C）

**文件：** `framework/core/math.h`

- 将 `#include <SimpleMath.h>` 替换为 `#include <h3d_simplemath_compat.hpp>`
- `using float2/3/4/float4x4` 别名不变
- `Math::Normalize / Cross / LookAt / Perspective / Ortho` 包装不变

**文件：** `framework/scene/camera/camera.cpp`（第 48、60 行）

- 两处直接调用 `DirectX::SimpleMath::Matrix::Create*` 改为走 `Math` 包装，降低未来耦合

---

## CMake 集成

**文件：** `third_party/CMakeLists.txt`

```cmake
add_library(h3d_math STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/h3d_math/src/h3d_math.c)
target_include_directories(h3d_math PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/h3d_math/include)

# 原 simplemath 链接替换为 h3d_math
# target_link_libraries(horizon_third_party PUBLIC simplemath)  ← 删除
target_link_libraries(horizon_third_party PUBLIC h3d_math)
```

Windows 下不再构建 / 链接 `directxtk12/src/SimpleMath.cpp`；Android / macOS 统一走 `h3d_math`，移除平台分叉。

---

## 新增文件

```
third_party/h3d_math/
├── include/
│   ├── h3d_math.h                  ← C Core 类型与函数声明
│   └── h3d_simplemath_compat.hpp   ← C++ SimpleMath 兼容层
├── src/
│   └── h3d_math.c                  ← C Core 实现
└── tests/
    └── h3d_math_test.cpp           ← 数值回归测试（可选）
```

---

## 实施优先级（分阶段）

| 阶段 | 内容 | 目标 |
|------|------|------|
| Phase 0 | 创建 `h3d_math` 库骨架，不切换入口 | 库单独可编译 |
| Phase 1 | 实现 `h3d_simplemath_compat.hpp` 最小子集 | 补齐工程实际用到的 API |
| Phase 2 | 切换 `math.h` 引用，修正 `camera.cpp` | `framework` 不再依赖 `SimpleMath.h` |
| Phase 3 | 全量编译 + 场景烟测 | 相机、meshlet、骨骼表现无回归 |
| Phase 4 | 移除残余 `SimpleMath` include 与链接项 | 彻底清理 |

---

## 风险与控制

| 风险 | 控制措施 |
|------|---------|
| 矩阵行列主序 / 坐标系不一致导致渲染错位 | 保持现有乘法顺序；增加视图投影快照测试 |
| `float3` 内存布局变化影响 `memcpy` 路径 | `static_assert(sizeof(Vector3) == 12)` 且标准布局 |
| 求逆实现差异导致动画 / 相机漂移 | 加入逆矩阵数值回归测试（epsilon `1e-5`） |

---

## 验证标准

### 编译
- Windows Debug / Release 通过
- Android 构建通过
- macOS 构建通过

### 数值（epsilon `1e-5`）
- `normalize / dot / cross` 基础向量测试
- `mat × mat_inv ≈ identity`
- `LookAt / Perspective / Ortho` 与旧实现关键元素一致

### 集成
- 相机矩阵正确（无镜像 / 翻转 / 深度异常）
- 网格 / 蒙皮无明显形变回归
- `framework` 内不再出现 `#include <SimpleMath.h>`
- CMake 中 `simplemath` 链接被 `h3d_math` 完全替代
