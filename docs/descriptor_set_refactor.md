# Vulkan Descriptor Set 重构 PRD

## 背景

当前 `VulkanDescriptorSetAllocator` 存在三类问题，在多平台（特别是 Android / macOS MoltenVK）上触发 crash 和内存异常：

1. **硬编码魔数**：pool size 2048、maxSets 1024/2048、`k_max_bindless_resources` 65536 等无依据的固定值，在高端 GPU 上导致巨型 pool 分配失败
2. **Crash 风险**：pool 耗尽时 `CHECK_VK_RESULT` 直接 assert；bindless set 写入无越界检查；`std::map::at()` 可抛 `out_of_range`
3. **不符合 Vulkan best practice**：per-frame pool 和 bindless pool 共享 reset 生命周期；bindless pool 应持久存在，不应每帧 reset；set 索引映射与 shader `set = N` 不对齐

参考规范：
- [Vulkan Descriptor Indexing Guide](https://docs.vulkan.org/guide/latest/extensions/VK_EXT_descriptor_indexing.html)
- [Khronos Descriptor Management Sample](https://github.khronos.org/Vulkan-Site/samples/latest/samples/performance/descriptor_management/README.html)
- [vkAllocateDescriptorSets](https://registry.khronos.org/vulkan/specs/latest/man/html/vkAllocateDescriptorSets.html)
- [vkResetDescriptorPool](https://registry.khronos.org/vulkan/specs/latest/man/html/vkResetDescriptorPool.html)

---

## 目标

- 消除已知 crash 点，所有可恢复 Vulkan 错误改为 log + fallback
- descriptor pool 按真实设备能力动态分配，移除魔数
- per-frame pool 支持自动增长，不再单次分配失败即崩
- bindless pool 生命周期持久化，不参与帧级 reset
- set 索引映射与 shader `set = N` 严格对齐

## 非目标

- 本阶段不覆盖 set2 及以上的多 set 扩展
- 不引入 `config.toml` 等运行时配置项覆盖 pool 参数
- 不改动 `vulkan_descriptor_set.h/cpp`、`vulkan_pipeline.cpp`、`vulkan_command_list.cpp` 的 API surface

---

## 设计约束（Vulkan 规范要求）

1. `pSetLayouts[s]` 必须对应 shader 的 `set = s`，绑定时 `firstSet + descriptorSetCount <= setLayoutCount`
2. `VARIABLE_DESCRIPTOR_COUNT` 只能用于 layout 内最后一个 binding
3. `UPDATE_AFTER_BIND` 需要 layout / binding / pool flag 与对应 feature 全部匹配
4. `vkAllocateDescriptorSets` 必须处理 `VK_ERROR_OUT_OF_POOL_MEMORY` / `VK_ERROR_FRAGMENTED_POOL`
5. `vkResetDescriptorPool` 会隐式释放该 pool 的所有 descriptor set，bindless set 不能参与帧级 reset

---

## 修改方案

### 修改 1：Pipeline Layout set 索引对齐

**文件：** `framework/rhi/vulkan/vulkan_pipeline.cpp` — `CreatePipelineLayout()`

`CreatePipelineLayout` 固定构建 set0 / set1 两个槽位，缺失的 set 用空 layout 填充，保证 `layouts[0]` 对应默认 set，`layouts[1]` 对应 bindless set，与 shader `set = N` 严格对应。

### 修改 2：命名常量替换全部魔数

**文件：** `vulkan_descriptor_set_allocator.h`

```cpp
static constexpr u32 k_max_bindless_resources              = 16384;
static constexpr u32 k_max_bindless_sets                   = 16;
static constexpr u32 k_initial_per_frame_max_sets          = 64;
static constexpr u32 k_per_frame_descriptors_per_type_mult = 4;
```

### 修改 3：Bindless pool sizes 按设备能力推导并 clamp

**文件：** `vulkan_descriptor_set_allocator.cpp` — `InitializeUpdateAfterBindLimits()`

```cpp
// 以设备上限为基础，clamp 到 k_max_bindless_resources
m_max_uab_uniform_buffers = std::min(raw_limit, k_max_bindless_resources);
// 同理其他 descriptor type
```

同时满足 `maxUpdateAfterBindDescriptorsInAllPools` 全局上界，必要时按比例缩放。

### 修改 4：Set0（per-frame）改为 growing pool 策略

**文件：** `vulkan_descriptor_set_allocator.h` / `.cpp`

- `m_temp_descriptor_pool` → `std::vector<PoolState> m_default_pools`，每个池记录 `maxSets` 和各 type 容量
- 初始 pool `maxSets = k_initial_per_frame_max_sets`，各 type 容量 = `maxSets × k_per_frame_descriptors_per_type_mult`
- `GetDescriptorSet()` 分配失败时：
  - `OUT_OF_POOL_MEMORY` / `FRAGMENTED_POOL`：几何增长创建新 pool，重试
  - 其他错误：`LOG_ERROR` + 返回 `nullptr`，不再 `CHECK_VK_RESULT` 崩溃

### 修改 5：Set1（bindless）改为全局共享 + 持久池

**文件：** `vulkan_descriptor_set_allocator.cpp`

- 缓存键从 `pipeline*` 改为 `bindless_layout_hash`（同 layout 全局共享 1 份 set）
- Bindless pool 生命周期改为持久，仅析构时销毁
- `ResetDescriptorPool()` 只 reset per-frame pools，不再 reset bindless pool
- 新 layout 触发分配不足时：重建 bindless pool + 重新分配 + 回放缓存 write

### 修改 6：Bindless variable-count 与多 binding 对齐

**文件：** `vulkan_descriptor_set_allocator.cpp`

- 每个 binding 的 `descriptorCount` 来自 SPIR-V reflection，不再写死 1
- 仅最后一个 binding 可加 `VARIABLE_DESCRIPTOR_COUNT_BIT`
- 多 bindless 数组场景（如 `vertex_buffers[] + material_textures[]`）：非最后 binding 使用固定容量，最后 binding 使用 variable-count
- 创建 layout 前校验对应 `descriptorBinding*UpdateAfterBind` feature；不满足则 pipeline 创建阶段报错，禁用该 bindless layout

### 修改 7：越界写入防御

**文件：** `vulkan_descriptor_set.cpp` — `SetBindlessResource()`

- 写入前按 binding 容量截断：超出容量时截断并输出一次性告警（资源名 / 请求数量 / 容量 / pipeline key）
- `resource.empty()` 继续保持"不写 0-count descriptor"逻辑

### 修改 8：防御性 API 修复

**文件：** `vulkan_descriptor_set_allocator.cpp`

- `descriptors.at(DEFAULT/BINDLESS)` 改为 `find` + 早返回，避免 `std::out_of_range`
- `CHECK_VK_RESULT` 仅保留不可恢复初始化错误，其余改为 log + fallback

---

## DescriptorDesc 类型扩展

**文件：** `framework/rhi/enums.h`

```cpp
struct DescriptorDesc {
    // ... 现有字段 ...
    u32  descriptor_count  = 1;      // 新增：数组大小（0 = runtime array）
    bool is_runtime_array  = false;  // 新增：是否 variable-count
};
```

**文件：** `vulkan_spirv_reflect.cpp`

从 `SpvReflectDescriptorBinding` 填充上述字段；`descriptor_count == 0` 视为 runtime array。

---

## 涉及文件

| 文件 | 改动内容 |
|------|---------|
| `framework/rhi/enums.h` | `DescriptorDesc` 新增 `descriptor_count` / `is_runtime_array` |
| `framework/rhi/vulkan/vulkan_spirv_reflect.cpp` | 填充新字段 |
| `framework/rhi/pipeline.cpp` | 跨 stage 合并 reflection 时校验一致性 |
| `framework/rhi/vulkan/vulkan_pipeline.cpp` | `CreatePipelineLayout` set 索引对齐 |
| `framework/rhi/vulkan/vulkan_descriptor_set_allocator.h` | 常量定义；`m_default_pools` vector；`CreatePerFramePool()` 私有方法 |
| `framework/rhi/vulkan/vulkan_descriptor_set_allocator.cpp` | 全部逻辑修改（见上） |
| `framework/rhi/vulkan/vulkan_descriptor_set.cpp` | `SetBindlessResource` 越界截断 |

**不改动**（API surface 不变）：`vulkan_descriptor_set.h`、`vulkan_command_list.cpp`

---

## 实施优先级

| 优先级 | 任务 |
|-------|------|
| P0 | 修改 7、8：消除已知 crash 路径（截断越界写、防 `out_of_range`） |
| P0 | 修改 1：Pipeline Layout set 索引对齐 |
| P1 | 修改 2、3：命名常量 + 设备能力推导 pool size |
| P1 | 修改 4：per-frame growing pool |
| P2 | 修改 5：bindless 持久池 + 全局共享 |
| P2 | 修改 6：variable-count 多 binding 对齐 |
| P3 | DescriptorDesc 类型扩展 + reflection 填充 |

---

## 验证标准

### 编译
- `cmake --build --preset msvcwin64` 通过
- `cmake --build --preset android_framework` 通过
- `cmake --build --preset macos_clang` 通过

### 运行
- deferred sample 正常渲染，无 crash
- 日志中 "Per-frame descriptor pool exhausted" 不出现（或仅出现一次后稳定）
- 日志中 bindless pool size ≤ 16384，非原始设备上限

### 生命周期
- 帧循环 reset 后 bindless set 仍有效（未被 reset 回收）
- renderer 销毁后 pool / layout / set 全部释放，无内存泄漏（Vulkan validation layer 无告警）

### 场景覆盖
| 测试场景 | 预期结果 |
|---------|---------|
| `set0 only` pipeline | 无 set1，layout 合法，渲染正常 |
| `set1 only` pipeline | set0 用空 layout 占位，绑定不崩 |
| 单 bindless 数组 | variable-count 正常分配与更新 |
| 多 bindless 数组（mesh path） | 两个 binding 均可写入，不因 `descriptorCount=1` 崩溃 |
| 超量 bindless 写入 | 截断告警，渲染不中断 |
| pool 压力（大量 pipeline） | set0 pool 自动增长，不崩溃 |
