# Vulkan Descriptor Set Allocator 重构方案

## Context

当前 `VulkanDescriptorSetAllocator` 存在三类问题：
1. **硬编码魔数**：pool size 2048、maxSets 1024/2048、k_max_bindless_resources 65536 等无依据的固定值
2. **Crash 风险**：pool 耗尽时 `CHECK_VK_RESULT` 直接 assert crash，无恢复机制；bindless pool 直接使用设备 UAB 上限（可达 500K+）作为 pool size，内存不足时创建失败
3. **不符合 best practice**：per-frame pool 和 bindless pool 共享相同的 reset 生命周期；bindless pool 应该是持久的，不应每帧 reset

## 修改方案

### 修改 1：Cap bindless pool sizes

**文件：** `framework/rhi/vulkan/vulkan_descriptor_set_allocator.cpp` — `InitializeUpdateAfterBindLimits()`

将原始设备限制 clamp 到 `k_max_bindless_resources`(16384)，避免在高端 GPU 上分配巨型 pool：

```cpp
m_max_uab_uniform_buffers = std::min(raw_limit, k_max_bindless_resources);
// 同理其他 4 个类型
```

### 修改 2：Bindless pool maxSets 改为合理值

**文件：** `vulkan_descriptor_set_allocator.cpp` — `CreateBindlessDescriptorPool()`

`maxSets` 从 1024 改为 `k_max_bindless_sets = 4`。实际只有几个 unique bindless 4 已有很大余量。

### 修改 3：Per-frame pool 改为 growing pool 策略

**文件：** `vulkan_descriptor_set_allocator.h` + `.cpp`

将单一 `m_temp_descriptor_pool` 替换为 `std::vector<VkDescriptorPool> m_per_frame_pools`。

- 初始创建 maxSets=64 的小 pool
- `GetDescriptorSet()` 中，若 `vkAllocateDescriptorSets` 返回 `VK_ERROR_OUT_OF_POOL_MEMORY`，翻倍创建新 pool 并重试
- 每 type 的 descriptor 数量 = maxSets × 4（每 set 平均 4 个同类型 descriptor 的保守估计）

### 修改 4：分离 per-frame / bindless pool 生命周期

**文件：** `vulkan_descriptor_set_allocator.cpp` — `ResetDescriptorPool()`

- 只 reset per-frame pools + 清理 `allocated_descriptorsets`
- **不再 reset `m_bindless_descriptor_pool`**，不再清理 `allocated_bindless_descriptorsets`
- Bindless sets 使用 UPDATE_AFTER_BIND，是持久资源

### 修改 5：整理析构函数

销毁 `m_per_frame_pools` vector 中所有 pool，保持 bindless pool 单独销毁。

### 修改 6：所有魔数提取为命名常量

```cpp
static constexpr u32 k_max_bindless_resources = 16384;
static constexpr u32 k_max_bindless_sets = 16;
static constexpr u32 k_initial_per_frame_max_sets = 64;
static constexpr u32 k_per_frame_descriptors_per_type_multiplier = 4;
```

## 涉及文件

| 文件 | 改动 |
|------|------|
| `framework/rhi/vulkan/vulkan_descriptor_set_allocator.h` | `m_temp_descriptor_pool` → `m_per_frame_pools` vector；添加常量；添加 `CreatePerFramePool()` 私有方法 |
| `framework/rhi/vulkan/vulkan_descriptor_set_allocator.cpp` | InitializeUpdateAfterBindLimits 加 clamp；CreateDescriptorPool → CreatePerFramePool；GetDescriptorSet 加 pool-growth retry；CreateBindlessDescriptorPool 改 maxSets；ResetDescriptorPool 不再 reset bindless；析构适配 pool vector |

不改动的文件：`vulkan_descriptor_set.h/cpp`、`vulkan_pipeline.cpp`、`vulkan_command_list.cpp`（API surface 不变）

## 验证

1. 编译通过：`cmake --build --preset msvcwin64`
2. 运行 deferred sample，确认正常渲染无 crash
3. 确认日志中 "Per-frame descriptor pool exhausted" 警告不出现（或仅出现一次然后稳定）
4. 确认日志中 bindless pool sizes 为 capped 值（≤16384），非原始设备限制

# Vulkan DescriptorSet（Set0/Set1）重构方案（去硬编码 + 防崩溃 + 对齐 Best Practice）

## Summary
目标是把当前 `set0`（默认）与 `set1`（bindless）的 `layout/pool/allocate/update` 逻辑改为“按真实需求和设备能力驱动”，并消除已知 crash 点。  
关键约束按 Vulkan 官方规范与 Khronos sample 落地：

1. `pSetLayouts[s]` 必须对应 shader 的 `set = s`，绑定时 `firstSet + descriptorSetCount <= setLayoutCount`。  
2. `VARIABLE_DESCRIPTOR_COUNT` 只能用于最后一个 binding。  
3. `UPDATE_AFTER_BIND` 需要 layout/binding/pool flag 与对应 feature 全部匹配。  
4. `vkAllocateDescriptorSets` 需要处理 `VK_ERROR_OUT_OF_POOL_MEMORY / VK_ERROR_FRAGMENTED_POOL`。  
5. `vkResetDescriptorPool` 会隐式释放该 pool 的所有 descriptor set，bindless set 不应做“每帧 reset 模式”。

参考：
- Vulkan Guide Descriptor Indexing: https://docs.vulkan.org/guide/latest/extensions/VK_EXT_descriptor_indexing.html
- Vulkan Sample Descriptor Management: https://github.khronos.org/Vulkan-Site/samples/latest/samples/performance/descriptor_management/README.html
- Vulkan Spec（set 映射）: https://docs.vulkan.org/spec/latest/chapters/interfaces.html
- `VkDescriptorSetLayoutBindingFlagsCreateInfo`（variable count 限制）: https://registry.khronos.org/vulkan/specs/latest/man/html/VkDescriptorSetLayoutBindingFlagsCreateInfo.html
- `vkAllocateDescriptorSets`: https://registry.khronos.org/vulkan/specs/latest/man/html/vkAllocateDescriptorSets.html
- `vkResetDescriptorPool`: https://registry.khronos.org/vulkan/specs/latest/man/html/vkResetDescriptorPool.html
- `VkDescriptorPoolCreateInfo`（`maxUpdateAfterBindDescriptorsInAllPools`）: https://registry.khronos.org/vulkan/specs/latest/man/html/VkDescriptorPoolCreateInfo.html

Skill 说明：本任务不匹配 `skill-creator/skill-installer`，不使用 skills。

---

## Important API / Type Changes
1. 在 [`enums.h`](d:/Codes/horizon/framework/rhi/enums.h) 扩展 `DescriptorDesc`：
- 新增 `u32 descriptor_count`（默认 1）
- 新增 `bool is_runtime_array`（默认 false）

2. 在 [`vulkan_spirv_reflect.cpp`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_spirv_reflect.cpp) 填充上述字段：
- 从 `SpvReflectDescriptorBinding` 读取数组信息
- `descriptor_count==0` 视为 runtime array（用于 variable-count 判定）

3. 在 [`pipeline.cpp`](d:/Codes/horizon/framework/rhi/pipeline.cpp) 合并 reflection 时：
- 同名资源跨 shader stage 必须校验 `set/binding/type` 一致
- `descriptor_count` 取可兼容值（固定数组取最大；runtime 优先）

---

## Implementation Plan

### 1) 修正 Pipeline Layout 的 set 索引映射（先做，直接去掉一类崩溃）
修改 [`vulkan_pipeline.cpp`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_pipeline.cpp)：
1. `CreatePipelineLayout` 固定构建 `set0/set1` 两个槽位（缺失的 set 用空 layout 填充）。
2. 保证 `layouts[0]` 对应默认 set，`layouts[1]` 对应 bindless set。
3. 维持命令侧绑定不变（`firstSet=0/1`），但不会再出现“set1 存在而 setLayoutCount=1 且只放在 index0”的非法映射。

### 2) Set0（默认集）改为“按需求增长”的可恢复分配器
修改 [`vulkan_descriptor_set_allocator.h`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_descriptor_set_allocator.h) / [`vulkan_descriptor_set_allocator.cpp`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_descriptor_set_allocator.cpp)：
1. `m_temp_descriptor_pool` -> `std::vector<PoolState> m_default_pools`（每个池记录 `maxSets` 与各 type 容量）。
2. 首次池大小按“当前要分配的 layout 真实 descriptor 需求”计算（不再 2048）。
3. `GetDescriptorSet` 分配失败时：
- 若 `OUT_OF_POOL_MEMORY/FRAGMENTED_POOL`：创建更大 pool（几何增长），重试。
- 其他错误：记录错误并返回 `nullptr`（不再直接 `CHECK` 崩溃）。
4. `ReleaseDescriptorSets` 与析构按池来源正确回收/销毁。

### 3) Set1（bindless）改为“按 layout 全局共享 + 持久池”
修改 [`vulkan_descriptor_set_allocator.cpp`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_descriptor_set_allocator.cpp)：
1. 缓存键从 `pipeline*` 改为 `bindless_layout_hash`（同 layout 全局共享 1 份 set）。
2. bindless pool 生命周期改为持久（仅析构销毁），`ResetDescriptorPool()` 不再 reset bindless pool。
3. bindless pool size 由设备能力推导：
- 以 `maxDescriptorSetUpdateAfterBind*` 为 per-type 上界
- 同时满足 `maxUpdateAfterBindDescriptorsInAllPools` 全局上界（必要时按比例缩放）
- 不再使用 `65536/1024` 这类固定值
4. 新 layout 触发分配不足时：可恢复扩容（重建 bindless pool + 重新分配共享 set + 回放缓存写入）。

### 4) Set1 多绑定数组与 variable-count 规则对齐
修改 [`vulkan_descriptor_set_allocator.cpp`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_descriptor_set_allocator.cpp)：
1. set1 每个 binding 的 `descriptorCount` 来自 reflection（或设备预算分配），不再统一写死 1。
2. 仅最后一个 binding 可加 `VARIABLE_DESCRIPTOR_COUNT_BIT`。
3. 多 bindless 数组场景（如 `vertex_buffers[] + material_textures[]`）：
- 非最后 binding 使用固定容量（>1）
- 最后 binding 使用 variable-count（如适用）
4. 创建 layout 前校验对应 `descriptorBinding*UpdateAfterBind` feature；不满足则在 pipeline 创建阶段报错并禁用该 bindless layout（防运行时 crash）。

### 5) 更新路径防越界（解决高频 crash 点）
修改 [`vulkan_descriptor_set.cpp`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_descriptor_set.cpp)：
1. `SetBindlessResource(...)` 写入前按 binding 容量截断（你选择的策略：截断并告警）。
2. `resource.empty()` 继续保持“不写 0-count descriptor”逻辑。
3. 记录一次性告警（资源名、请求数量、容量、pipeline/layout key），便于定位资源超配。

### 6) 防御性修复（避免异常终止）
修改 [`vulkan_descriptor_set_allocator.cpp`](d:/Codes/horizon/framework/rhi/vulkan/vulkan_descriptor_set_allocator.cpp)：
1. `descriptors.at(DEFAULT/BINDLESS)` 改为 `find` + 早返回，避免 `std::out_of_range`。
2. 所有可恢复 Vulkan 错误路径改为“日志 + fallback/返回空”，保留 assert 仅用于不可恢复初始化错误。

---

## Test Cases / Scenarios

1. `set0 only` pipeline：
- 无 set1，`CreatePipelineLayout` 仍合法，`BindDescriptorSets(set0)` 正常。

2. `set1 only` pipeline：
- set0 缺失但 set1 存在时，layout 仍按 index 对齐（set0 空 layout，占位）。

3. 单 bindless 数组（`material_textures[]`）：
- variable-count 正常分配与更新。

4. 多 bindless 数组（mesh path：`vertex_buffers[] + material_textures[]`）：
- 两个 binding 都可写入；
- 非最后 binding 不再因 `descriptorCount=1` 崩溃。

5. 超量写入：
- `SetBindlessResource` 输入数量 > 容量时不崩溃，发生截断告警。

6. pool 压力测试：
- 人工创建大量 pipeline/layout，验证 set0 pool 自动增长；
- 不出现 `CHECK_VK_RESULT(vkAllocateDescriptorSets)` 触发崩溃。

7. 生命周期测试：
- 帧循环 reset 后 bindless set 仍有效（未被 reset 回收）；
- renderer 销毁时 pool/layout/set 全部释放，无泄漏。

---

## Explicit Assumptions / Defaults
1. 仅聚焦 `set0` 与 `set1`（当前引擎约定）。  
2. `set1` 采用“按 bindless layout 全局共享”策略。  
3. bindless 超量写入默认“截断并告警”，不中断渲染。  
4. 参数来源采用“仅设备极限推导”，不新增 `config.toml` 覆盖项。  
5. 如果设备不支持所需 descriptor-indexing feature 组合，pipeline 创建阶段明确报错并禁用对应 bindless layout，避免运行时随机崩溃。
